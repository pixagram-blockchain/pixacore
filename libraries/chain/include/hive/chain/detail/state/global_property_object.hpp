#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <fc/uint128.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/balance.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

using hive::protocol::PIXA_asset;
using hive::protocol::PXS_asset;
using hive::protocol::PXS_price;
using hive::protocol::VEST_asset;
using hive::protocol::VEST_price;

/**
  * @class dynamic_global_property_object
  * @brief Maintains global state information
  * @ingroup object
  * @ingroup implementation
  *
  * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
  * current values of global blockchain properties.
  */
class dynamic_global_property_object : public object< dynamic_global_property_object_type, dynamic_global_property_object >
{
  CHAINBASE_OBJECT( dynamic_global_property_object );

public:
  template< typename Allocator >
  dynamic_global_property_object( allocator< Allocator > a, uint64_t _id,
    const account_name_type& _initial_witness )
    : id( _id ), current_witness( _initial_witness )
  {}

  //main PIXA token counter
  const PIXA_asset& get_current_supply() const { return current_supply; }
  //creates PIXA out of thin air - for inflation
  inline temp_PIXA_balance issue_PIXA( const PIXA_asset& pixa_amount );
  //burns all PIXA on given balance into the void - for clearing null
  inline void burn_PIXA( temp_PIXA_balance& pixa_balance );
  //all PIXA in existence including those that would be created if all PXS was converted to PIXA
  const PIXA_asset& get_virtual_supply() const { return virtual_supply; }

  PIXA_asset& access_current_supply() { return current_supply; } // TODO: should not be needed once issue/burn/convert are used
  PIXA_asset& access_virtual_supply() { return virtual_supply; } // TODO: should not be needed once issue/burn/convert are used

  //main PXS token counter
  const PXS_asset& get_current_pxs_supply() const { return current_pxs_supply; }
  //initial PXS tokens
  const PXS_asset& get_initial_pxs_supply() const { return init_pxs_supply; }
  //creates PXS out of thin air - for interest
  inline temp_PXS_balance issue_PXS( const PXS_asset& pxs_amount, const PXS_price& exchange_rate, bool full_update_virtual_supply = false );
  //burns all PXS on given balance into the void - for clearing null
  inline void burn_PXS( temp_PXS_balance& pxs_balance, const PXS_price& exchange_rate, bool full_update_virtual_supply = false );
  //converts all PIXA on given balance to PXS
  inline temp_PXS_balance convert_PIXA_to_PXS( temp_PIXA_balance& pixa_balance, const PXS_price& exchange_rate, bool full_update_virtual_supply = true );
  //Pixagram HF30: converts all PIXA on given balance into exactly pxs_amount of PXS, leaving virtual_supply alone
  //(DPF funding carries the part below 0.001 PXS between blocks, so the amount is not hive * exchange_rate)
  inline temp_PXS_balance convert_PIXA_to_exact_PXS( temp_PIXA_balance& pixa_balance, const PXS_asset& pxs_amount );

  PXS_asset& access_current_pxs_supply() { return current_pxs_supply; } //TODO: should not be needed with init/burn/convert
  PXS_asset& access_initial_pxs_supply() { return init_pxs_supply; } //TODO: should not be needed with proper genesis rework

  //rate of interest for holding PXS (in BPS - basis points)
  uint16_t get_pxs_interest_rate() const { return pxs_interest_rate; }
  //percentage of PIXA being converted to PXS during payouts (in BPS - basis points)
  uint16_t get_pxs_print_rate() const { return pxs_print_rate; }

  void set_pxs_interest_rate( uint16_t rate ) { pxs_interest_rate = rate; }
  void set_pxs_print_rate( uint16_t rate ) { pxs_print_rate = rate; }

  //pool of PIXA tokens vested normally
  const PIXA_asset& get_total_vesting_fund_pixa() const { return total_vesting_fund_pixa; }

  // Hive's names of four getters, kept for code built on the node (HAF's sql_serializer)
  const PXS_asset& get_current_hbd_supply() const { return get_current_pxs_supply(); }
  const PXS_asset& get_initial_hbd_supply() const { return get_initial_pxs_supply(); }
  uint16_t get_hbd_interest_rate() const { return get_pxs_interest_rate(); }
  const PIXA_asset& get_total_vesting_fund_hive() const { return get_total_vesting_fund_pixa(); }
  //amount of VESTS produced from PIXA held in normal vested fund
  const VEST_asset& get_total_vesting_shares() const { return total_vesting_shares; }
  //issues a specified amount of VESTS - used internally, also for use in debug plugin and unit tests (used f.e. for initial vest price stabilization)
  inline temp_VEST_balance issue_VESTS( const VEST_asset& vest_amount, bool is_reward = false );
  //burns all VESTS on given balance - used internally, also for clearing null
  inline void burn_VESTS( temp_VEST_balance& vest_balance, bool is_reward = false );
  //creates new VESTS out of all PIXA on given balance, PIXA is moved to total_vesting_fund_pixa if not is_reward
  inline temp_VEST_balance convert_PIXA_to_VESTS( temp_PIXA_balance& pixa_balance, bool is_reward = false );
  //creates new VESTS out of all PIXA on given balance (with use of passed price), PIXA is moved to total_vesting_fund_pixa if not is_reward
  inline temp_VEST_balance convert_PIXA_to_VESTS( temp_PIXA_balance& pixa_balance, const VEST_price& vest_price, bool is_reward = false );
  //burns all VESTS on given balance and gives out associated PIXA from total_vesting_fund_pixa
  temp_PIXA_balance convert_VESTS_to_PIXA( temp_VEST_balance& vest_balance ) { return convert_VESTS_to_PIXA( vest_balance, get_vesting_share_price() ); }
  //burns all VESTS on given balance (with use of passed price) and gives out associated PIXA from total_vesting_fund_pixa
  inline temp_PIXA_balance convert_VESTS_to_PIXA( temp_VEST_balance& vest_balance, const VEST_price& vest_price );

  HIVE_balance& access_total_vesting_fund_pixa() { return total_vesting_fund_pixa; } //TODO: should not be needed when convert is defined and used
  VEST_asset& access_total_vesting_shares() { return total_vesting_shares; } //TODO: should not be needed when convert is defined and used

  VEST_price get_vesting_share_price() const
  {
    if ( total_vesting_fund_pixa.is_empty() || total_vesting_shares.amount == 0 )
      return VEST_price( 1000000, 1000 );
    return VEST_price( total_vesting_shares, total_vesting_fund_pixa );
  }

  //pool of PIXA tokens for pending (vested) rewards
  const PIXA_asset& get_pending_rewarded_vesting_pixa() const { return pending_rewarded_vesting_pixa; }
  //amount of VESTS produced from PIXA held in pending reward vested fund
  const VEST_asset& get_pending_rewarded_vesting_shares() const { return pending_rewarded_vesting_shares; }

  PIXA_asset& access_pending_rewarded_vesting_pixa() { return pending_rewarded_vesting_pixa; } //TODO: should not be needed when convert is defined and used
  VEST_asset& access_pending_rewarded_vesting_shares() { return pending_rewarded_vesting_shares; } //TODO: should not be needed when convert is defined and used

  VEST_price get_reward_vesting_share_price() const
  {
    return VEST_price( total_vesting_shares + pending_rewarded_vesting_shares,
      total_vesting_fund_pixa.as_asset() + pending_rewarded_vesting_pixa );
  }

  const PXS_asset& get_dhf_interval_ledger() const { return dhf_interval_ledger; }
  time_point_sec get_next_maintenance_time() const { return next_maintenance_time; }
  time_point_sec get_last_budget_time() const { return last_budget_time; }
  time_point_sec get_next_daily_maintenance_time() const { return next_daily_maintenance_time; }

  void add_to_dhf_interval_ledger( const PXS_asset& value ) { dhf_interval_ledger += value; }
  void clear_dhf_interval_ledger() { dhf_interval_ledger = PXS_asset( 0 ); }
  void set_next_maintenance_time( time_point_sec now, int64_t next_maintenance_delta_seconds )
  {
    last_budget_time = now;
    next_maintenance_time = now + fc::seconds( next_maintenance_delta_seconds );
  }
  void set_next_daily_maintenance_time( time_point_sec now, int64_t next_maintenance_delta_seconds )
  {
    next_daily_maintenance_time = now + fc::seconds( next_maintenance_delta_seconds );
  }

  uint32_t get_head_block_number() const { return head_block_number; }
  const block_id_type& get_head_block_id() const { return head_block_id; }
  time_point_sec get_head_block_time() const { return time; }
  const account_name_type& get_current_witness() const { return current_witness; }
  uint64_t get_current_aslot() const { return current_aslot; }
  fc::uint128_t get_recent_slots_filled() const { return recent_slots_filled; }
  uint8_t get_participation_count() const { return participation_count; }

  void set_new_head_block( uint32_t number, const block_id_type& id, time_point_sec ts )
  {
    head_block_number = number;
    head_block_id = id;
    time = ts;
  }
  void set_current_witness( const account_name_type& witness ) { current_witness = witness; }
  void set_current_aslot( uint64_t slot ) { current_aslot = slot; }
  void set_recent_slots_filled( fc::uint128_t filled_slots_bitmap ) { recent_slots_filled = filled_slots_bitmap; }
  void set_participation_count( uint8_t count ) { participation_count = count; }

  uint64_t get_total_pow() const { return total_pow; }
  uint32_t get_current_pow_witnesses() const { return num_pow_witnesses; }

  void on_pow_witness() { ++total_pow; ++num_pow_witnesses; } // when new pow is handled
  void remove_pow_witness() { --num_pow_witnesses; } // when miner is removed from queue

  uint32_t get_maximum_block_size() const { return maximum_block_size; }
  void set_maximum_block_size( uint32_t size ) { maximum_block_size = size; }

// various settings only changed during hardforks

  uint32_t get_vote_power_reserve_rate() const { return vote_power_reserve_rate; }
  uint32_t get_voting_mana_regeneration_seconds() const { return voting_mana_regeneration_seconds; }
  uint32_t get_delegation_return_period() const { return delegation_return_period; }
  uint64_t get_reverse_auction_seconds() const { return reverse_auction_seconds; }
  uint64_t get_early_voting_seconds() const { return early_voting_seconds; }
  uint64_t get_mid_voting_seconds() const { return mid_voting_seconds; }
  int64_t get_available_account_subsidies() const { return available_account_subsidies; }
  uint16_t get_pxs_stop_percent() const { return pxs_stop_percent; }
  uint16_t get_pxs_start_percent() const { return pxs_start_percent; }
  uint16_t get_content_reward_percent() const { return content_reward_percent; }
  uint16_t get_vesting_reward_percent() const { return vesting_reward_percent; }
  uint16_t get_proposal_fund_percent() const { return proposal_fund_percent; }
  uint16_t get_downvote_pool_percent() const { return downvote_pool_percent; }
  int16_t get_current_remove_threshold() const { return current_remove_threshold; }
  uint8_t get_max_consecutive_recurrent_transfer_failures() const { return max_consecutive_recurrent_transfer_failures; }
  uint16_t get_max_recurrent_transfer_end_date() const { return max_recurrent_transfer_end_date; }
  uint8_t get_min_recurrent_transfers_recurrence() const { return min_recurrent_transfers_recurrence; }
  uint16_t get_max_open_recurrent_transfers() const { return max_open_recurrent_transfers; }

  void set_vote_power_reserve_rate( uint32_t value ) { vote_power_reserve_rate = value; }
  void set_voting_mana_regeneration_seconds( uint32_t value ) { voting_mana_regeneration_seconds = value; }
  void set_delegation_return_period( uint32_t value ) { delegation_return_period = value; }
  void set_reverse_auction_seconds( uint64_t value ) { reverse_auction_seconds = value; }
  void set_early_voting_seconds( uint64_t value ) { early_voting_seconds = value; }
  void set_mid_voting_seconds( uint64_t value ) { mid_voting_seconds = value; }
  void set_available_account_subsidies( int64_t value ) { available_account_subsidies = value; }
  void set_pxs_stop_percent( uint16_t value ) { pxs_stop_percent = value; }
  void set_pxs_start_percent( uint16_t value ) { pxs_start_percent = value; }
  void set_content_reward_percent( uint16_t value ) { content_reward_percent = value; }
  void set_vesting_reward_percent( uint16_t value ) { vesting_reward_percent = value; }
  void set_proposal_fund_percent( uint16_t value ) { proposal_fund_percent = value; }
  void set_downvote_pool_percent( uint16_t value ) { downvote_pool_percent = value; }
  void set_current_remove_threshold( int16_t value ) { current_remove_threshold = value; }
  void set_max_consecutive_recurrent_transfer_failures( uint8_t value ) { max_consecutive_recurrent_transfer_failures = value; }
  void set_max_recurrent_transfer_end_date( uint16_t value ) { max_recurrent_transfer_end_date = value; }
  void set_min_recurrent_transfers_recurrence( uint8_t value ) { min_recurrent_transfers_recurrence = value; }
  void set_max_open_recurrent_transfers( uint16_t value ) { max_open_recurrent_transfers = value; }

  // dgpo is never removed so this routine should never be called
  void check_on_remove() const
  {
    FC_ASSERT( total_vesting_fund_pixa.is_empty(), "Removing dgpo with non-empty balance field" );
  }

private:
  uint32_t          head_block_number = 0;
  block_id_type     head_block_id;
  time_point_sec    time = HIVE_GENESIS_TIME;
  account_name_type current_witness; //< TODO: check if worth replacing with account_id_type (ABW: probably not)

  //The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
  uint64_t total_pow = -1;

  //The current count of how many pending POW witnesses there are, determines the difficulty of doing pow
  uint32_t num_pow_witnesses = 0;

  PIXA_asset   virtual_supply;
  PIXA_asset   current_supply;
  PXS_asset    init_pxs_supply;
  PXS_asset    current_pxs_supply;
  HIVE_balance total_vesting_fund_pixa;
  VEST_asset   total_vesting_shares;
  VEST_asset   pending_rewarded_vesting_shares;
  PIXA_asset   pending_rewarded_vesting_pixa; // not a balance, just total sum of account_object::reward_vesting_pixa

  uint16_t pxs_interest_rate = 0;
  uint16_t pxs_print_rate = HIVE_100_PERCENT;

  /**
    *  Maximum block size is decided by the set of active witnesses which change every round.
    *  Each witness posts what they think the maximum size should be as part of their witness
    *  properties, the median size is chosen to be the maximum block size for the round.
    *
    *  @note the minimum value for maximum_block_size is defined by the protocol to prevent the
    *  network from getting stuck by witnesses attempting to set this too low.
    */
  uint32_t     maximum_block_size = HIVE_MAX_BLOCK_SIZE;

  /**
    * The current absolute slot number.  Equal to the total
    * number of slots since genesis.  Also equal to the total
    * number of missed slots plus head_block_number.
    */
  uint64_t      current_aslot = 0;

  /**
    * used to compute witness participation.
    */
  fc::uint128_t recent_slots_filled = fc::uint128_max_value();
  uint8_t       participation_count = 128; ///< Divide by 128 to compute participation percentage

  /**
    * The number of votes regenerated per day.  Any user voting slower than this rate will be
    * "wasting" voting power through spillover; any user voting faster than this rate will
    * eventually exhaust their voting mana (since HF28, previously their vote was reduced).
    */
  uint32_t vote_power_reserve_rate = HIVE_INITIAL_VOTE_POWER_RATE;

  /**
    * Time an empty voting bar (and the downvote bar) takes to refill. HIVE_VOTING_MANA_REGENERATION_SECONDS
    * (5 days) until hardfork 32, PIXA_HF32_VOTING_MANA_REGENERATION_SECONDS after.
    */
  uint32_t voting_mana_regeneration_seconds = HIVE_VOTING_MANA_REGENERATION_SECONDS;

  uint32_t delegation_return_period = HIVE_DELEGATION_RETURN_PERIOD_HF0;

  uint64_t reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF6;
  uint64_t early_voting_seconds = 0;
  uint64_t mid_voting_seconds = 0;

  int64_t available_account_subsidies = 0;

  uint16_t pxs_stop_percent = HIVE_PXS_STOP_PERCENT_HF14;
  uint16_t pxs_start_percent = HIVE_PXS_START_PERCENT_HF14;

  // Settings used to compute payments for every proposal
  time_point_sec next_maintenance_time = HIVE_GENESIS_TIME;
  time_point_sec last_budget_time = HIVE_GENESIS_TIME;
  // Setting to convert hive to PXS in the treasury account
  time_point_sec next_daily_maintenance_time = HIVE_GENESIS_TIME;

  uint16_t content_reward_percent = HIVE_CONTENT_REWARD_PERCENT_HF16;
  uint16_t vesting_reward_percent = HIVE_VESTING_FUND_PERCENT_HF16;
  uint16_t proposal_fund_percent = HIVE_PROPOSAL_FUND_PERCENT_HF0;

  PXS_asset dhf_interval_ledger = PXS_asset( 0 );

  uint16_t downvote_pool_percent = 0;

  // limits number of objects removed in one automatic operation (only applies to situations where many
  // objects can accumulate over time but need to be removed in single operation f.e. proposal votes)
  int16_t current_remove_threshold = HIVE_GLOBAL_REMOVE_THRESHOLD; //negative means no limit

  uint8_t max_consecutive_recurrent_transfer_failures = HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES;
  uint16_t max_recurrent_transfer_end_date = HIVE_MAX_RECURRENT_TRANSFER_END_DATE;
  uint8_t min_recurrent_transfers_recurrence = HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE;
  uint16_t max_open_recurrent_transfers = HIVE_MAX_OPEN_RECURRENT_TRANSFERS;

  CHAINBASE_UNPACK_CONSTRUCTOR( dynamic_global_property_object );
};

inline temp_PIXA_balance dynamic_global_property_object::issue_PIXA( const PIXA_asset& pixa_amount )
{
  temp_PIXA_balance pixa_balance;
  pixa_balance.issue_asset( pixa_amount );
  current_supply += pixa_amount;
  virtual_supply += pixa_amount;
  return pixa_balance;
}

inline void dynamic_global_property_object::burn_PIXA( temp_PIXA_balance& pixa_balance )
{
  const PIXA_asset& pixa_amount = pixa_balance;
  current_supply -= pixa_amount;
  virtual_supply -= pixa_amount;
  pixa_balance.burn_asset( pixa_amount );
}

inline temp_PXS_balance dynamic_global_property_object::issue_PXS( const PXS_asset& pxs_amount, const PXS_price& exchange_rate, bool full_update_virtual_supply )
{
  temp_PXS_balance pxs_balance;
  pxs_balance.issue_asset( pxs_amount );
  current_pxs_supply += pxs_amount;
  if( full_update_virtual_supply )
    virtual_supply = current_pxs_supply * exchange_rate + current_supply;
  else
    virtual_supply += pxs_amount * exchange_rate;
  return pxs_balance;
}

inline void dynamic_global_property_object::burn_PXS( temp_PXS_balance& pxs_balance, const PXS_price& exchange_rate, bool full_update_virtual_supply )
{
  const PXS_asset& pxs_amount = pxs_balance;
  current_pxs_supply -= pxs_amount;
  if( full_update_virtual_supply )
    virtual_supply = current_pxs_supply * exchange_rate + current_supply;
  else
    virtual_supply -= pxs_amount * exchange_rate;
  pxs_balance.burn_asset( pxs_amount );
}

inline temp_PXS_balance dynamic_global_property_object::convert_PIXA_to_PXS( temp_PIXA_balance& pixa_balance, const PXS_price& exchange_rate, bool full_update_virtual_supply )
{
  temp_PXS_balance pxs_balance;
  const PIXA_asset& pixa_amount = pixa_balance;
  PXS_asset pxs_amount = pixa_amount * exchange_rate;
  pxs_balance.issue_asset( pxs_amount );
  current_pxs_supply += pxs_amount;
  current_supply -= pixa_amount;
  pixa_balance.burn_asset( pixa_amount );
  //virtual_supply does not change in principle, but this routine is a replacement for one that did full update which means it repaired prior accumulated truncations
  if( full_update_virtual_supply )
    virtual_supply = current_pxs_supply * exchange_rate + current_supply;
  return pxs_balance;
}

inline temp_PXS_balance dynamic_global_property_object::convert_PIXA_to_exact_PXS( temp_PIXA_balance& pixa_balance, const PXS_asset& pxs_amount )
{
  temp_PXS_balance pxs_balance;
  const PIXA_asset& pixa_amount = pixa_balance;
  pxs_balance.issue_asset( pxs_amount );
  current_pxs_supply += pxs_amount;
  current_supply -= pixa_amount;
  pixa_balance.burn_asset( pixa_amount );
  return pxs_balance;
}

inline temp_VEST_balance dynamic_global_property_object::issue_VESTS( const VEST_asset& vest_amount, bool is_reward )
{
  temp_VEST_balance vest_balance;
  vest_balance.issue_asset( vest_amount );
  if( is_reward )
    pending_rewarded_vesting_shares += vest_amount;
  else
    total_vesting_shares += vest_amount;
  return vest_balance;
}

inline void dynamic_global_property_object::burn_VESTS( temp_VEST_balance& vest_balance, bool is_reward )
{
  const VEST_asset& vest_amount = vest_balance;
  if( is_reward )
    pending_rewarded_vesting_shares -= vest_amount;
  else
    total_vesting_shares -= vest_amount;
  vest_balance.burn_asset( vest_amount );
}

inline temp_VEST_balance dynamic_global_property_object::convert_PIXA_to_VESTS( temp_PIXA_balance& pixa_balance, bool is_reward )
{
  VEST_price vest_price = is_reward ? get_reward_vesting_share_price() : get_vesting_share_price();
  return convert_PIXA_to_VESTS( pixa_balance, vest_price, is_reward );
}

inline temp_VEST_balance dynamic_global_property_object::convert_PIXA_to_VESTS( temp_PIXA_balance& pixa_balance, const VEST_price& vest_price, bool is_reward )
{
  const PIXA_asset& pixa_amount = pixa_balance;
  VEST_asset vest_amount = pixa_amount * vest_price;
  if( is_reward )
    pending_rewarded_vesting_pixa += pixa_amount;
  else
    total_vesting_fund_pixa.transfer_from( pixa_balance );
  return issue_VESTS( vest_amount, is_reward );
}

inline temp_PIXA_balance dynamic_global_property_object::convert_VESTS_to_PIXA( temp_VEST_balance& vest_balance, const VEST_price& vest_price )
{
  temp_PIXA_balance pixa_balance;
  const VEST_asset& vest_amount = vest_balance;
  PIXA_asset pixa_amount = vest_amount * vest_price;
  pixa_balance.transfer_from( total_vesting_fund_pixa, pixa_amount );
  burn_VESTS( vest_balance );
  return pixa_balance;
}

} } // hive::chain

FC_REFLECT( hive::chain::dynamic_global_property_object,
          (id)
          (head_block_number)
          (head_block_id)
          (time)
          (current_witness)
          (total_pow)
          (num_pow_witnesses)
          (virtual_supply)
          (current_supply)
          (init_pxs_supply)
          (current_pxs_supply)
          (total_vesting_fund_pixa)
          (total_vesting_shares)
          (pending_rewarded_vesting_shares)
          (pending_rewarded_vesting_pixa)
          (pxs_interest_rate)
          (pxs_print_rate)
          (maximum_block_size)
          (current_aslot)
          (recent_slots_filled)
          (participation_count)
          (vote_power_reserve_rate)
          (voting_mana_regeneration_seconds)
          (delegation_return_period)
          (reverse_auction_seconds)
          (early_voting_seconds)
          (mid_voting_seconds)
          (available_account_subsidies)
          (pxs_stop_percent)
          (pxs_start_percent)
          (next_maintenance_time)
          (last_budget_time)
          (next_daily_maintenance_time)
          (content_reward_percent)
          (vesting_reward_percent)
          (proposal_fund_percent)
          (dhf_interval_ledger)
          (downvote_pool_percent)
          (current_remove_threshold)
          (max_consecutive_recurrent_transfer_failures)
          (max_recurrent_transfer_end_date)
          (max_open_recurrent_transfers)
          (min_recurrent_transfers_recurrence)
        )
