#pragma once

#include <hive/chain/util/asset.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/misc_utilities.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::share_type;
using hive::protocol::PXS_asset;
using hive::protocol::PIXA_asset;
using hive::protocol::PXS_price;

using fc::uint128_t;

struct comment_reward_context
{
  share_type         rshares;
  uint16_t           reward_weight = 0;
  PXS_asset          max_pxs;
  uint128_t          total_reward_shares2 = 0;
  PIXA_asset         total_reward_fund_pixa;
  PXS_price          current_pixa_price;
  protocol::curve_id reward_curve = protocol::quadratic;
  uint128_t          content_constant = HIVE_CONTENT_CONSTANT_HF0;
};

PIXA_asset get_rshare_reward( const comment_reward_context& ctx );

inline uint128_t get_content_constant_s()
{
  return HIVE_CONTENT_CONSTANT_HF0; // looking good for posters
}

uint128_t evaluate_reward_curve( const uint128_t& rshares, const protocol::curve_id& curve = protocol::quadratic, const uint128_t& var1 = HIVE_CONTENT_CONSTANT_HF0 );

inline bool is_comment_payout_dust( const PXS_price& p, const PIXA_asset& pixa_payout )
{
  return to_pxs( p, pixa_payout ) < HIVE_MIN_PAYOUT_PXS;
}

} } } // hive::chain::util

FC_REFLECT( hive::chain::util::comment_reward_context,
  (rshares)
  (reward_weight)
  (max_pxs)
  (total_reward_shares2)
  (total_reward_fund_pixa)
  (current_pixa_price)
  (reward_curve)
  (content_constant)
  )
