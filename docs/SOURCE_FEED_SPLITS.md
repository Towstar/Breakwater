# Connecting production to several tanks

Select the production source in **Network** or the scene, click **Connect**, then
select the receiving tank. If the source already has a feed, the split editor
opens with an equal-share proposal. Review the percentages and flows, then choose
**Apply split**. **Cancel** leaves the network unchanged.

For Production B at 16 m³/h, a 50/50 split supplies 8 m³/h to each tank. A 75/25
split supplies 12 and 4 m³/h. Production reduction applies before the split; reducing
production by 2 m³/h with a 50/50 split supplies 7 m³/h to each tank.

To change an existing allocation, select the source and use **Edit feed split**,
or select a source pipe and use **Edit source feed split**. Percentages have two
decimal places and must total exactly 100%. A 0% branch is inactive. Existing pipe
IDs, costs and quality parameters remain attached to their routes. The editor
does not normalize an invalid total or silently delete an existing feed.

These are configured flow allocations. The model does not calculate a pressure
or valve-resistance split. Each branch still has its own capacity limit. Numerical
or topology edits retire old forecasts and approval; moving equipment preserves
eligible results. Forecasts continue to display infeasible inventories without
clipping them, and only native validation permits simulated operator approval.

Save split projects in the updated application. They use project version 3;
older applications reject them. Legacy single-feed projects keep version 1 and
their physical identity. Version 2 remains reserved for the separate G1 telemetry
contract. See [the allocation contract](../contracts/SOURCE_FEEDS.md) for exact
compatibility and scientific-worker rules.
