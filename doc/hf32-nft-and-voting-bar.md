# HF32: native NFTs and the voting bar

Hardfork 32 has no activation time yet: `HIVE_HARDFORK_1_32_TIME` keeps the far-future default,
like HF31, which has to be scheduled no later than HF32 (`database::init_hardforks` refuses a
schedule that puts HF32 first). Both are compiled into every build (`HIVE_NUM_HARDFORKS` is 32).

## Native NFTs

`nft_operation` (37) mints a root post as an NFT with 1 to 10,000 copies for 1.000 PXS per copy,
burned, and lists, unlists, buys and transfers its copies; edition 0 is the reserve, the copies
that never left the author, offered at one price. See [devs/operations/37_nft.md](devs/operations/37_nft.md).

## The voting bar

| | Until HF32 | From HF32 |
|---|---|---|
| Full vote | 2% of the bar (`vote_power_reserve_rate` 10) | 3% of the bar (rate 5) |
| Refill (voting and downvote bars) | 5 days, 20% a day | 576,000 s = 6 days 16 hours, 15% a day |
| Delegation return | 432,000 s (5 days) | 604,800 s (7 days) |
| Smallest vote, dust deduction, downvote pool, resource credit refill | unchanged | unchanged |

The refill time becomes the global property `voting_mana_regeneration_seconds`, read by
`util::update_manabar` (both bars) and by `hf20_vote_evaluator`; `get_dynamic_global_properties`
of `database_api` and `condenser_api` return it. `HIVE_VOTING_MANA_REGENERATION_SECONDS` stays
at 5 days for the pre-HF20 vote rule, for replay, and for `HIVE_RC_REGEN_TIME`.

At the fork block every account's bars are first brought up to date at the old rate, then the
three values are written: mana that accrued before the fork is credited at the rate it accrued
at. Delegations removed before the fork keep their return date. The resource credit price of
removing a delegation follows the return period in force (`count_resources` gets it from the
global properties), so it is unchanged before HF32 and covers 7 days after.

`condenser_api` computes the legacy `voting_power` field of accounts with the refill time in
force. Front ends that hard-code 2% or 5 days have to read `vote_power_reserve_rate` and
`voting_mana_regeneration_seconds` instead.

## Upgrading a node

The release changes the state layout (a field of the global properties, two NFT indexes), so a
node replays or loads a snapshot made by the new version. The blocks before the fork replay as
before.

## Tests

`tests/unit/tests/hf32_tests.cpp`: `voting_bar` (values before and after, the settling of a bar at
the fork, delegations returned before and after, the cost of a full and a 1% vote, the refill of a
day, a rejected vote, a downvote taking the pool then the bar) and the NFT cases, `reserve_offer`
included. `operation_tests.cpp` reads the refill time and the return period from the global
properties instead of the 5-day macro.
