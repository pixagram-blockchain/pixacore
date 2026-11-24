#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/util/balance.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

class account_object;

using hive::protocol::PIXA_asset;
using hive::protocol::PXS_asset;
using hive::protocol::asset;

class escrow_object : public object< escrow_object_type, escrow_object >
{
  CHAINBASE_OBJECT( escrow_object );
public:
  template< typename Allocator >
  escrow_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _from, const account_object& _to, const account_object& _agent,
    temp_PIXA_balance&& _pixa_amount, temp_PXS_balance&& _pxs_amount, temp_balance&& _fee,
    const time_point_sec& _ratification_deadline, const time_point_sec& _escrow_expiration, uint32_t _escrow_transfer_id )
  : id( _id ), escrow_id( _escrow_transfer_id ),
    ratification_deadline( _ratification_deadline ), escrow_expiration( _escrow_expiration ),
    pxs_balance( std::move( _pxs_amount ) ), pixa_balance( std::move( _pixa_amount ) ), pending_fee( std::move( _fee ) )
  {
    init( _from, _to, _agent );
  }

// getters:

  uint32_t get_escrow_id() const { return escrow_id; }
  const account_name_type& get_from() const { return from; }
  const account_name_type& get_to() const { return to; }
  const account_name_type& get_agent() const { return agent; }
  time_point_sec get_ratification_deadline() const { return ratification_deadline; }
  time_point_sec get_escrow_expiration() const { return escrow_expiration; }

  //PIXA portion of transfer balance
  const PIXA_asset& get_pixa_balance() const { return pixa_balance; }
  //PXS portion of transfer balance
  const PXS_asset& get_pxs_balance() const { return pxs_balance; }
  //fee offered to escrow (can be either in PIXA or PXS)
  const asset& get_fee() const { return pending_fee; }

  bool is_to_approved() const { return to_approved; }
  bool is_agent_approved() const { return agent_approved; }
  bool is_approved() const { return to_approved && agent_approved; }
  bool is_disputed() const { return disputed; }

  void check_on_remove() const
  {
    FC_ASSERT( pixa_balance.is_empty() && pxs_balance.is_empty() && pending_fee.is_empty(),
      "Removing escrow_object with non-empty balance fields" );
  }

// setters:

  HIVE_balance& access_pixa_balance() { return pixa_balance; }
  PXS_balance& access_pxs_balance() { return pxs_balance; }
  balance& access_fee() { return pending_fee; }

  void approve_to() { to_approved = true; }
  void approve_agent() { agent_approved = true; }
  void start_dispute() { disputed = true; }

private:
  uint32_t          escrow_id = 20;
  account_name_type from; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
  account_name_type to; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
  account_name_type agent; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
  time_point_sec    ratification_deadline;
  time_point_sec    escrow_expiration;
  PXS_balance       pxs_balance;
  HIVE_balance      pixa_balance;
  balance           pending_fee; //fee can use PIXA of PXS
  bool              to_approved = false; //< TODO: can be replaced with bit field along with all flags
  bool              agent_approved = false;
  bool              disputed = false;

  void init( const account_object& _from, const account_object& _to, const account_object& _agent );

  CHAINBASE_UNPACK_CONSTRUCTOR( escrow_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::escrow_object,
          (id)(escrow_id)(from)(to)(agent)
          (ratification_deadline)(escrow_expiration)
          (pxs_balance)(pixa_balance)(pending_fee)
          (to_approved)(agent_approved)(disputed) )
