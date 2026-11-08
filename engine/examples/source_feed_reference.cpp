// Private executed source-feed fixtures, independent of Qt and worker protocols.
#include "../tests/source_feed_fixture.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>
#include <locale>
#include <cmath>
#include <stdexcept>
using namespace breakwater;
namespace {
void number(std::ostream& o,double value) { if(!std::isfinite(value)) throw std::runtime_error("Nonfinite source-feed fixture"); o<<value; }
void string(std::ostream& o,const std::string& s) { o<<std::quoted(s); }
void numbers(std::ostream& o,const std::vector<double>& v) {o<<'[';for(std::size_t i=0;i<v.size();++i){if(i)o<<',';number(o,v[i]);}o<<']';}
void network(std::ostream& o,const Network& n) {
    o<<"{\"assets\":[";
    for(std::size_t i=0;i<n.assets.size();++i){if(i)o<<',';const auto& a=n.assets[i];
        o<<"{\"id\":";string(o,a.id);o<<",\"label\":";string(o,a.label);o<<",\"kind\":";string(o,kind_name(a.kind));
        for(const auto& [key,value]:std::vector<std::pair<const char*,double>>{{"capacity_m3",a.capacity_m3},{"initial_m3",a.initial_m3},{"inflow_m3h",a.inflow_m3h},{"curtailment_m3h",a.curtailment_m3h},{"curtailment_max_m3h",a.curtailment_max_m3h},{"cost_per_m3",a.cost_per_m3}}){o<<',';string(o,key);o<<':';number(o,value);}o<<'}';}
    o<<"],\"routes\":[";
    for(std::size_t i=0;i<n.routes.size();++i){if(i)o<<',';const auto& r=n.routes[i];
        o<<"{\"id\":";string(o,r.id);o<<",\"source_id\":";string(o,r.source_id);o<<",\"target_id\":";string(o,r.target_id);
        o<<",\"capacity_m3h\":";number(o,r.capacity_m3h);o<<",\"setpoint_m3h\":";number(o,r.setpoint_m3h);
        o<<",\"cost_per_m3\":";number(o,r.cost_per_m3);o<<",\"has_pump\":false,\"available\":true,\"source_share_bps\":"<<r.source_share_bps<<'}';}
    o<<"]}";
}
void result(std::ostream& o,const CompiledNetwork& c,Solver solver){
    const SimulationConfig config{3,.025};const auto r=simulate(c,config,solver);const auto v=validate(c,r,config);
    o<<"{\"tank_ids\":[";for(std::size_t i=0;i<r.tank_ids.size();++i){if(i)o<<',';string(o,r.tank_ids[i]);}
    o<<"],\"time_h\":";numbers(o,r.time_h);o<<",\"volumes_m3\":[";
    for(std::size_t i=0;i<r.volumes_m3.size();++i){if(i)o<<',';numbers(o,r.volumes_m3[i]);}
    o<<"],\"cost\":";number(o,r.cost);o<<",\"feasible\":"<<(v.feasible?"true":"false")<<",\"mass_residual_m3\":";number(o,v.max_conservation_residual_m3);o<<'}';
}
}
int main(){try{
    auto zero=test::source_feed_network({0,10000},2);zero.routes[0].capacity_m3h=0;
    const std::vector<std::pair<std::string,Network>> cases{{"balanced",test::source_feed_network({5000,5000},0)},
        {"asymmetric",test::source_feed_network({7500,2500},2)},{"four_way",test::source_feed_network({1000,2000,3000,4000},4)},{"zero_share",zero}};
    std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(std::numeric_limits<double>::max_digits10);
    o<<"{\"fixture_version\":1,\"solver_version\":";string(o,solver_version);o<<",\"cases\":[";
    for(std::size_t i=0;i<cases.size();++i){if(i)o<<',';o<<"{\"id\":";string(o,cases[i].first);o<<",\"network\":";network(o,cases[i].second);
        o<<",\"horizon_h\":3,\"step_h\":0.025,\"solvers\":{";const auto c=compile(cases[i].second);
        std::size_t j=0;for(const auto& [name,solver]:std::vector<std::pair<const char*,Solver>>{{"sparse",Solver::sparse},{"dense",Solver::dense_reference},{"affine",Solver::affine}}){if(j++)o<<',';string(o,name);o<<':';result(o,c,solver);}o<<"}}";}
    o<<"]}\n";std::cout<<o.str();if(!std::cout)throw std::runtime_error("Failed source-feed publication");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
