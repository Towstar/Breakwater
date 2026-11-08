#pragma once
// Private verification inputs; analytical expectations live independently in tests.
#include "breakwater/engine.hpp"

namespace breakwater::test {
inline Network source_feed_network(const std::vector<std::uint32_t>& shares, double curtailment) {
    Network n;
    for (std::size_t i=0;i<shares.size();++i) {
        Asset tank; tank.id="tank_"+std::to_string(i); tank.label="Tank "+std::to_string(i+1);
        tank.kind=AssetKind::tank; tank.capacity_m3=200;
        tank.initial_m3=shares.size()==2 ? (i==0 ? 80.0 : 30.0) : 0.0;
        n.assets.push_back(tank);
        Route feed;feed.id="feed_"+std::to_string(i);feed.source_id="production";feed.target_id=tank.id;
        feed.capacity_m3h=16;feed.cost_per_m3=static_cast<double>(i+1);feed.source_share_bps=shares[i];
        n.routes.push_back(feed);
    }
    Asset source;source.id="production";source.label="Production B";source.kind=AssetKind::source;
    source.inflow_m3h=16;source.curtailment_m3h=curtailment;source.curtailment_max_m3h=8;
    source.cost_per_m3=shares.size()==4 ? 5.0 : 3.0;n.assets.push_back(source);
    return n;
}
} // namespace breakwater::test
