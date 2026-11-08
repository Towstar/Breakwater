#include "source_feed_fixture.hpp"
#include "breakwater/search.hpp"
#include "../src/control_influence.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
using namespace breakwater;
namespace {
void require(bool v,const char* message="Independent source-feed expectation failed"){if(!v)throw std::runtime_error(message);}
void near(double a,double b){require(std::isfinite(a)&&std::abs(a-b)<=1e-9+1e-10*std::max(std::abs(a),std::abs(b)));}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}require(caught,"Expected source-feed rejection");}
void trajectory(const Network& n,const std::vector<double>& terminal,double cost){
    const auto c=compile(n);const SimulationConfig config{3,.025};
    for(auto solver:{Solver::sparse,Solver::dense_reference,Solver::affine}){
        const auto r=simulate(c,config,solver);near(r.cost,cost);require(r.tank_ids.size()==terminal.size());
        for(std::size_t t=0;t<r.time_h.size();++t)for(std::size_t i=0;i<terminal.size();++i)
            near(r.volumes_m3[i][t],c.initial_m3[i]+(terminal[i]-c.initial_m3[i])*(r.time_h[t]/3));
        const auto v=validate(c,r,config);require(v.feasible);near(v.max_conservation_residual_m3,0);
        auto forged=r;forged.volumes_m3[0].back()+=1;require(!validate(c,forged,config).feasible);
        forged=r;forged.cost+=1;require(!validate(c,forged,config).feasible);
    }
}
}
int main(){std::size_t passed=0;try{
    const auto test=[&](const char* name,const std::function<void()>& f){f();++passed;std::cout<<"PASS "<<name<<'\n';};
    test("independent 8+8 trajectory and cost",[]{trajectory(breakwater::test::source_feed_network({5000,5000},0),{104,54},72);});
    test("independent 75/25 curtailed trajectory and cost",[]{trajectory(breakwater::test::source_feed_network({7500,2500},2),{111.5,40.5},70.5);});
    test("four-way trajectory and source cost once",[]{trajectory(breakwater::test::source_feed_network({1000,2000,3000,4000},4),{3.6,7.2,10.8,14.4},168);});
    test("zero-share zero-capacity branch",[]{auto n=breakwater::test::source_feed_network({0,10000},2);n.routes[0].capacity_m3h=0;trajectory(n,{80,72},102);});
    test("incorrect integer totals rejected",[]{for(auto shares:std::vector<std::vector<std::uint32_t>>{{5000,4999},{5000,5001},{0,0},{10001,0},{std::numeric_limits<std::uint32_t>::max(),1}}){auto n=breakwater::test::source_feed_network(shares,0);rejects([&]{compile(n);});}});
    test("duplicate tank source feed rejected",[]{auto n=breakwater::test::source_feed_network({5000,5000},0);n.routes[1].target_id=n.routes[0].target_id;rejects([&]{compile(n);});});
    test("source pumps and unavailable branches rejected",[]{auto n=breakwater::test::source_feed_network({5000,5000},0);n.routes[0].has_pump=true;rejects([&]{compile(n);});n.routes[0].has_pump=false;n.routes[0].available=false;rejects([&]{compile(n);});});
    test("every branch capacity checked at equality and beyond",[]{auto n=breakwater::test::source_feed_network({5000,5000},2);n.routes[1].capacity_m3h=7;require(inspect_network(n).empty());std::vector<ControlValue> c{{"source:production:curtail",0}};rejects([&]{compile(n,c);});});
    test("tank-route share metadata rejected",[]{auto n=breakwater::test::source_feed_network({5000,5000},0);Route r;r.id="transfer";r.source_id="tank_0";r.target_id="tank_1";r.capacity_m3h=10;r.source_share_bps=5000;n.routes.push_back(r);rejects([&]{compile(n);});});
    test("shares remain fixed configuration rather than route controls",[]{auto n=breakwater::test::source_feed_network({5000,5000},0);require(control_catalog(n).size()==1);std::vector<ControlValue> c{{"route:feed_0:flow",1}};rejects([&]{compile(n,c);});});
    test("influence map fans out to more than two tanks and preserves costs",[]{
        const auto n=breakwater::test::source_feed_network({1000,2000,3000,4000},4);detail::Prepared p(n);detail::CandidateSet set;set.bindings={p.find_control("source:production:curtail")};
        detail::ControlInfluence map(p,set);detail::InfluenceWorkspace fast(map);detail::Workspace slow(p);
        for(double curtail:{0.,2.,4.,8.}){detail::Values values{};values[0]=curtail;double a=0,b=0;
            require(map.screen(values,3,fast,a)==detail::screen_candidate(p,set,values,3,slow,b));near(a,b);near(a,144+6*curtail);
            for(std::size_t i=0;i<4;++i)near(fast.rates[i],slow.rates[i]);}
    });
    test("public search deterministic allocation-aware ranking",[]{
        const auto n=breakwater::test::source_feed_network({1000,2000,3000,4000},4);
        const std::vector<CandidateControls> candidates{{{{"source:production:curtail",4}}},{{{"source:production:curtail",0}}},{{{"source:production:curtail",8}}}};
        const auto r=evaluate_candidates(n,candidates,{{3,.025},5});require(r.recommendations.size()==3);
        require(r.recommendations[0].candidate_index==1&&r.recommendations[1].candidate_index==0&&r.recommendations[2].candidate_index==2);
        near(r.recommendations[0].run.cost,144);near(r.recommendations[1].run.cost,168);near(r.recommendations[2].run.cost,192);
    });
    std::cout<<passed<<'/'<<passed<<" source-feed checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<passed<<" checks: "<<e.what()<<'\n';return 1;}}
