#include "front_advection.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace breakwater;
using namespace breakwater::front_experiment;
namespace {
void require(bool v, const char* message = "Independent F1b expectation failed") { if (!v) throw std::runtime_error(message); }
void near(double a, double b, double atol=1e-11) { require(std::isfinite(a) && std::abs(a-b)<=atol); }
template<class E=std::invalid_argument, class F> void rejects(F f) {
    bool caught=false; try { f(); } catch (const E&) { caught=true; } require(caught,"Expected F1b rejection");
}
PipeConfig pipe(std::size_t n=2) { return {static_cast<double>(n),.2,0,0,n}; }
TransportConfig config(double h=.5, double output=.5) {
    TransportConfig c; c.horizon_h=h; c.output_step_h=output; c.cfl_safety=.5; return c;
}
Result solve(const PipeConfig& p, const std::vector<double>& u, TransportConfig c=config(),
             const std::vector<FlowKnot>& q={{0,.2}}, const std::vector<InletKnot>& in={{0,0}},
             const std::vector<double>& source={}) { return advect(p,u,q,in,c,source); }
void field(const Result& r, const std::vector<double>& expected) {
    require(r.transport.concentration_mg_l.back()==expected,"Hand-calculated binary stage values differ");
}
void ledger(const Result& r) {
    const auto& t=r.transport;
    require(t.solver_version==solver_version && r.stage_evaluations==2*t.diagnostics.substeps &&
            r.cell_stage_updates==r.stage_evaluations*t.x_m.size());
    for (std::size_t i=0; i<t.time_h.size(); ++i) {
        const double balance=t.inventory_mg[i]-t.inventory_mg[0]-t.cumulative_inlet_mg[i]+t.cumulative_outlet_mg[i];
        near(balance,0,2e-8); near(t.mass_balance_residual_mg[i],balance,2e-8);
    }
}
} // namespace
int main() {
    std::size_t passed=0;
    const auto test=[&](const char* name, const std::function<void()>& f) {
        try { f(); ++passed; std::cout<<"PASS "<<name<<'\n'; }
        catch (const std::exception& e) { std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; throw; }
    };
    try {
        test("MC fixed slopes and extrema",[]{
            require(mc_slope(.25,.5)==.375 && mc_slope(-.25,-.5)==-.375);
            require(mc_slope(.125,1)==.25 && mc_slope(1,.125)==.25);
            require(mc_slope(0,1)==0 && mc_slope(1,-1)==0);
            rejects<std::runtime_error>([]{ mc_slope(std::numeric_limits<double>::infinity(),1); });
            rejects<std::runtime_error>([]{ mc_slope(1e308,1e308); });
        });
        test("shared faces and first-order endpoints",[]{
            const std::vector<double> u{0,.25,.75,1};
            require(face_fluxes(u,1,2)==std::vector<double>({2,0,.4375,.9375,1}));
        });
        test("two-cell SSP stages outlet quadrature and liters",[]{
            const auto r=solve(pipe(),{1,0}); field(r,{.625,.25});
            near(r.transport.inventory_mg[0],200); near(r.transport.inventory_mg.back(),175);
            near(r.transport.cumulative_outlet_mg.back(),25); ledger(r);
            require(r.transport.diagnostics.max_cfl_number==.5 && r.stage_evaluations==2);
        });
        test("interior SSP hand calculation",[]{
            const auto r=solve(pipe(4),{0,.25,.75,1},config(.25,.25));
            field(r,{0,.16015625,.6181640625,.9736328125});
            near(r.transport.cumulative_outlet_mg.back(),49.609375); ledger(r);
        });
        test("stage-time linear inlet injection",[]{
            const auto r=solve(pipe(),{0,0},config(),{{0,.2}},{{0,0},{.5,1}});
            field(r,{.25,0}); near(r.transport.cumulative_inlet_mg.back(),50); ledger(r);
        });
        test("flow knot left interval both stages and paused state",[]{
            const auto r=solve(pipe(),{1,0},config(1.5,.5),{{0,.2},{.5,0},{1,.2}});
            require(r.transport.concentration_mg_l[1]==std::vector<double>({.625,.25}));
            require(r.transport.concentration_mg_l[2]==r.transport.concentration_mg_l[1]);
            field(r,{.390625,.3125}); ledger(r);
        });
        test("inlet knots and unequal event pieces",[]{
            const auto r=solve(pipe(),{0,0},config(.75,.75),{{0,.2}},{{0,0},{.25,1},{.5,0}});
            near(r.transport.cumulative_inlet_mg.back(),50); require(r.transport.diagnostics.substeps==3); ledger(r);
        });
        test("constant fields and zero coefficients",[]{
            for (std::size_t n : {2u,3u,64u,4096u}) {
                const auto r=solve(pipe(n),std::vector<double>(n,2),config(1,.125),{{0,.2},{.375,0},{.625,.4}},{{0,2}});
                field(r,std::vector<double>(n,2)); ledger(r);
            }
        });
        test("zero data and exact terminal time",[]{
            auto c=config(1.1,.3); auto r=solve(pipe(),{0,0},c,{{0,0}});
            require(r.transport.time_h==std::vector<double>({0,.3,.6,.8999999999999999,1.1})); field(r,{0,0});
        });
        test("rounded interior endpoint remains distinct",[]{
            const auto r=solve(pipe(),{0,0},config(.225,.075),{{0,0}});
            require(r.transport.time_h==std::vector<double>({0,.075,.15,3*.075,.225}));
            require(r.transport.time_h[3]<r.transport.time_h[4]);
        });
        test("smallest horizon has two endpoints",[]{
            const auto h=std::numeric_limits<double>::denorm_min();
            require(solve(pipe(),{0,0},config(h,24),{{0,0}}).transport.time_h==std::vector<double>({0,h}));
        });
        test("seeded bounded conservative profiles and input immutability",[]{
            std::mt19937 rng(14802); std::uniform_real_distribution<double> random(0,1);
            for (std::size_t n : {2u,3u,7u,64u}) {
                std::vector<double> u(n); for (double& v:u) v=random(rng);
                const auto captured=u; const double in=random(rng), maximum=std::max(in,*std::max_element(u.begin(),u.end()));
                const auto r=solve(pipe(n),u,config(3,.125),{{0,.2}},{{0,in}});
                require(u==captured && r.transport.diagnostics.minimum_concentration_mg_l>=-5e-12 &&
                        r.maximum_concentration_mg_l<=maximum+1e-10); ledger(r);
            }
        });
        test("sharp pulse no clipping or overshoot",[]{
            std::vector<double> u(64); for (std::size_t j=16;j<32;++j) u[j]=1;
            const auto r=solve(pipe(64),u,config(4,.125));
            require(r.maximum_concentration_mg_l<=1+1e-10 && r.transport.diagnostics.minimum_concentration_mg_l>=-5e-12); ledger(r);
        });
        test("unsupported physics rejected independently",[]{
            for (double v : {1.,-1.,std::numeric_limits<double>::quiet_NaN()}) {
                auto p=pipe(); p.dispersion_m2_h=v; rejects([&]{solve(p,{0,0});});
                p=pipe(); p.decay_per_h=v; rejects([&]{solve(p,{0,0});});
                rejects([&]{solve(pipe(),{0,0},config(),{{0,.2}},{{0,0}},{v,0});});
            }
            auto c=config();c.boundary=TransportBoundary::closed_no_flux; rejects([&]{solve(pipe(),{0,0},c);});
            rejects([]{solve(pipe(),{0,0},config(),{{0,.2}},{{0,0}},{0});});
        });
        test("CFL domain including nextafter equality",[]{
            for (double v : {0.,-.1,std::nextafter(.5,1.),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
                auto c=config(); c.cfl_safety=v; rejects([&]{solve(pipe(),{0,0},c);});
            }
        });
        test("invalid fields geometry and schedules",[]{
            for (double v : {-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
                rejects([&]{solve(pipe(),{v,0});});
                rejects([&]{solve(pipe(),{0,0},config(),{{0,v}});});
                rejects([&]{solve(pipe(),{0,0},config(),{{0,0}},{{0,v}});});
                auto p=pipe();p.area_m2=v;rejects([&]{solve(p,{0,0});});
            }
            rejects([]{solve(pipe(),{0});});
            rejects([]{solve(pipe(),{0,0},config(),{},{{0,0}});});
            rejects([]{solve(pipe(),{0,0},config(),{{0,0}},{});});
            rejects([]{solve(pipe(),{0,0},config(),{{0,0},{0,1}});});
            rejects([]{solve(pipe(),{0,0},config(),{{0,0}},{{.1,0}});});
            rejects([]{solve(pipe(),{0,0},config(),{{0,0}},{{0,0},{0,1}});});
            rejects([]{solve(pipe(1),{0});}); rejects([]{solve(pipe(4097),std::vector<double>(4097));});
        });
        test("nonfinite derived arithmetic and nonadvancing clock",[]{
            auto p=pipe();p.area_m2=1e-310;
            rejects<std::runtime_error>([&]{solve(p,{0,0},config(),{{0,1}});});
            p=pipe();p.length_m=std::numeric_limits<double>::denorm_min();
            rejects<std::runtime_error>([&]{solve(p,{0,0});});
            auto c=config();c.cfl_safety=std::numeric_limits<double>::denorm_min();
            rejects<std::runtime_error>([&]{solve(pipe(),{0,0},c,{{0,.4}});});
            rejects<std::runtime_error>([]{solve(pipe(),{1e308,0});});
        });
        test("configured step limit and exact stage accounting",[]{
            auto c=config(1,1); c.max_substeps=2; ledger(solve(pipe(),{1,0},c));
            c.max_substeps=1; rejects<std::runtime_error>([&]{solve(pipe(),{1,0},c);});
            check_step_budget(999999,2,1000000);
            rejects<std::runtime_error>([]{check_step_budget(1000000,2,1000000);});
            check_step_budget(6249,4000,1000000);
            rejects<std::runtime_error>([]{check_step_budget(6250,4000,1000000);});
        });
        test("exact frame limit quotient independent",[]{
            require(solve(pipe(),{0,0},config(1,.0001),{{0,0}}).transport.time_h.size()==10001);
            rejects([]{solve(pipe(),{0,0},config(std::nextafter(1.,2.),.0001),{{0,0}});});
        });
        test("exact four-million saved values",[]{
            const auto c=config(9999./8192,1./8192);
            const auto r=solve(pipe(400),std::vector<double>(400),c,{{0,0}});
            require(r.transport.concentration_mg_l.size()*400==max_transport_values);
            rejects([]{solve(pipe(400),std::vector<double>(400),config(10000./8192,1./8192),{{0,0}});});
        });
        test("actual fifty-million stage-update boundary",[]{
            const auto p=pipe(2500); const std::vector<double> u(2500);
            const auto r=solve(p,u,config(10000./8192,2),{{0,.2*4096}});
            require(r.cell_stage_updates==max_transport_cell_updates && r.transport.diagnostics.substeps==10000);
            rejects<std::runtime_error>([&]{solve(p,u,config(10001./8192,2),{{0,.2*4096}});});
        });
    } catch (...) { return 1; }
    std::cout<<passed<<"/"<<passed<<" experimental checks passed\n"; return 0;
}
