"""Independent allocation oracles; actual native/worker parity is a separate CLI."""
import copy
import math
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import torch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from breakwater_science.rollout import StorageModel
import worker_storage
import worker

def network(shares=(5000,5000),curtailment=0):
    n=len(shares)
    assets=[dict(id=f"tank_{i}",label=f"Tank {i+1}",kind="tank",capacity_m3=200.,
        initial_m3=(80. if i==0 else 30.) if n==2 else 0.) for i in range(n)]
    assets.append(dict(id="production",label="Production B",kind="source",inflow_m3h=16.,curtailment_m3h=curtailment,
        curtailment_max_m3h=8.,cost_per_m3=5. if n==4 else 3.))
    routes=[dict(id=f"feed_{i}",source_id="production",target_id=f"tank_{i}",capacity_m3h=16.,setpoint_m3h=0.,
        cost_per_m3=float(i+1),has_pump=False,available=True,source_share_bps=share) for i,share in enumerate(shares)]
    return dict(assets=assets,routes=routes)
def payload(n=None,derivatives=False):
    return dict(method_version=2,identity=dict(snapshot_id="split",volume_run_id="native",physical_hash="a"*64,numerical_revision=1),
        network=n or network(),controls=[],horizon_h=3.,step_h=.025,derivatives=derivatives,preview=dict(max_times=256))
def normalized(n):return copy.deepcopy(n)  # Fixture contains only tanks and sources: native/wire units coincide.
class AllocationTests(unittest.TestCase):
    def check(self,n,expected,cost):
        result=StorageModel(normalized(n)).rollout(horizon_h=3.,step_h=.025)
        torch.testing.assert_close(result.volumes_m3[:,-1],torch.tensor(expected,dtype=torch.float64),atol=1e-9,rtol=1e-10)
        self.assertAlmostEqual(result.cost.item(),cost,places=10)
        self.assertAlmostEqual(float(result.volumes_m3[:,-1].sum()-result.volumes_m3[:,0].sum()),3*(16-n["assets"][-1]["curtailment_m3h"]),places=10)
    def test_eight_plus_eight_is_not_sixteen_per_branch(self):self.check(network(),[104,54],72)
    def test_asymmetric_split_and_curtailment_cost_once(self):self.check(network((7500,2500),2),[111.5,40.5],70.5)
    def test_four_way_conservation_and_cost(self):self.check(network((1000,2000,3000,4000),4),[3.6,7.2,10.8,14.4],168)
    def test_zero_share_and_capacity_equality(self):
        n=network((0,10000),2);n["routes"][0]["capacity_m3h"]=0;self.check(n,[80,72],102)
    def test_invalid_totals_are_not_normalized(self):
        for shares in ((5000,4999),(5000,5001),(0,0),(10000,10000)):
            with self.subTest(shares=shares),self.assertRaises(ValueError):StorageModel(network(shares))
    def test_share_types_and_limits(self):
        for value in (True,-1,10001,5000.5,float("nan"),float("inf"),"5000"):
            n=network();n["routes"][0]["source_share_bps"]=value
            with self.subTest(value=value),self.assertRaises(ValueError):StorageModel(n)
    def test_duplicate_receiving_tank(self):
        n=network();n["routes"][1]["target_id"]="tank_0"
        with self.assertRaises(ValueError):StorageModel(n)
    def test_capacity_on_every_branch_and_curtailment_override(self):
        n=network(curtailment=2);n["routes"][1]["capacity_m3h"]=7
        m=StorageModel(n);m.rollout()
        with self.assertRaises(ValueError):m.rollout({"source:production:curtail":0.})
    def test_capture_is_immutable(self):
        n=network();m=StorageModel(n);before=m.rollout().volumes_m3
        n["routes"][0]["source_share_bps"]=0
        torch.testing.assert_close(m.rollout().volumes_m3,before,atol=0,rtol=0)
    def test_source_routes_are_not_flow_controls(self):
        m=StorageModel(network());self.assertEqual(m.control_ids,("source:production:curtail",))
        with self.assertRaises(ValueError):m.rollout({"route:feed_0:flow":1.})
    def test_analytical_jacobian_and_three_central_differences(self):
        m=StorageModel(network((1000,2000,3000,4000),4))
        def f(value):
            r=m.rollout({"source:production:curtail":value},horizon_h=3.,step_h=3.)
            return torch.cat((r.volumes_m3[:,-1],r.cost.reshape(1)))
        x=torch.tensor(4.,dtype=torch.float64,requires_grad=True)
        actual=torch.autograd.functional.jacobian(f,x);expected=torch.tensor([-.3,-.6,-.9,-1.2,6.],dtype=torch.float64)
        torch.testing.assert_close(actual,expected,atol=1e-10,rtol=1e-10)
        for h in (1e-3,1e-4,1e-5):torch.testing.assert_close((f(x+h)-f(x-h))/(2*h),expected,atol=1e-7,rtol=1e-8)
class WorkerAllocationTests(unittest.TestCase):
    def rejected_before_torch(self,p):
        with patch.object(worker_storage,"_load_backend") as loader:
            with self.assertRaises(worker_storage.StorageError):worker_storage.storage_rollout(p)
            loader.assert_not_called()
    def test_v1_does_not_silently_ignore_shares(self):
        p=payload();p["method_version"]=1;self.rejected_before_torch(p)
    def test_bad_share_totals_and_types_rejected_before_runtime(self):
        for value in (True,5000.1,-1,10001,4999,float("inf")):
            p=payload();p["network"]["routes"][0]["source_share_bps"]=value
            with self.subTest(value=value):self.rejected_before_torch(p)
    def test_method_version_echo_and_fixed_expectations(self):
        r=worker_storage.storage_rollout(payload(network((7500,2500),2)))
        self.assertEqual(r["method_version"],2);self.assertEqual(r["tank_ids"],["tank_0","tank_1"])
        self.assertAlmostEqual(r["volumes_m3"][0][-1],111.5,places=10);self.assertAlmostEqual(r["volumes_m3"][1][-1],40.5,places=10)
        self.assertAlmostEqual(r["cost"],70.5,places=10);self.assertEqual(r["time_indices"][-1],120)
    def test_all_capacity_boundaries_checked_for_derivatives(self):
        p=payload(network(curtailment=2),True);p["controls"]=[dict(control_id="source:production:curtail",value=2.)]
        p["network"]["routes"][1]["capacity_m3h"]=7
        self.rejected_before_torch(p)
    def test_zero_share_capacity_is_not_a_false_control_boundary(self):
        p=payload(network((0,10000),2),True);p["network"]["routes"][0]["capacity_m3h"]=0
        p["controls"]=[dict(control_id="source:production:curtail",value=2.)]
        r=worker_storage.storage_rollout(p)
        self.assertEqual(r["derivatives"]["terminal_volume_jacobian_h"],[[0.],[-3.]])
        self.assertEqual(r["derivatives"]["cost_gradient"],[3.])
    def test_four_way_worker_derivatives(self):
        p=payload(network((1000,2000,3000,4000),4),True);p["controls"]=[dict(control_id="source:production:curtail",value=4.)]
        r=worker_storage.storage_rollout(p)
        for a,b in zip(r["derivatives"]["terminal_volume_jacobian_h"],(-.3,-.6,-.9,-1.2)):self.assertAlmostEqual(a[0],b,places=13)
        self.assertAlmostEqual(r["derivatives"]["cost_gradient"][0],6.,places=12)
    def test_legacy_method_one_still_works(self):
        n=network((10000,),0);n["routes"][0].pop("source_share_bps");p=payload(n);p["method_version"]=1
        r=worker_storage.storage_rollout(p);self.assertEqual(r["method_version"],1);self.assertAlmostEqual(r["volumes_m3"][0][-1],48.,places=11)
    def test_publication_rejects_wrong_method_version(self):
        p=payload();r=worker_storage.storage_rollout(p);r["method_version"]=1
        with self.assertRaises(worker._WorkerError):worker._storage_result(r,p)
    def test_stdlib_capability_advertises_highest_storage_version(self):
        r=worker.handle(dict(protocol_version=1,request_id="caps",method="capabilities",payload={"capability_version":1}))
        self.assertEqual(next(m for m in r["result"]["methods"] if m["name"]=="storage_rollout")["method_version"],2)
if __name__=="__main__":unittest.main()
