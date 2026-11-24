#pragma once

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive::protocol {
  struct pxs_interest
  {
    static uint128_t evaluate_pxs_interest(uint128_t* pxs_seconds, const time_point_sec& head_block_time, const PXS_asset& pxs, const time_point_sec& pxs_seconds_last_update,
                                           uint16_t pxs_interest_rate, bool do_evaluation)
    {
      *pxs_seconds += uint128_t(pxs.amount.value) * (head_block_time - pxs_seconds_last_update).to_seconds();

      if (*pxs_seconds != 0 && do_evaluation)
      {
        auto interest = *pxs_seconds / HIVE_SECONDS_PER_YEAR;
        interest *= pxs_interest_rate;
        interest /= HIVE_100_PERCENT;
        return interest;
      }

      return 0;
    }
  };

} // hive::protocol
