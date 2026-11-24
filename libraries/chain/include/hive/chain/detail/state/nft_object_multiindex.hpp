#pragma once
#include <hive/chain/detail/state/nft_object.hpp>

namespace hive { namespace chain {

  struct by_nft_permlink;
  struct by_nft_edition;
  struct by_nft_owner;

  /**
    * by_nft_permlink is the identity of an NFT: one minted post per (creator, permlink). It answers
    * "is this post minted" without reading the post, which matters because posts older than the
    * payout window live in the comment archive, and it lists the NFTs a given account created.
    */
  typedef multi_index_container<
    nft_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< nft_object, nft_object::id_type, &nft_object::get_id > >,
      ordered_unique< tag< by_nft_permlink >,
        composite_key< nft_object,
          const_mem_fun< nft_object, account_id_type, &nft_object::get_creator >,
          const_mem_fun< nft_object, const shared_string&, &nft_object::get_permlink >
        >,
        composite_key_compare< std::less< account_id_type >, chainbase::strcmp_less >
      >
    >,
    multi_index_allocator< nft_object >
  > nft_index;

  /**
    * Only the copies that were offered or moved at least once are here (see nft_object). by_nft_edition
    * finds one copy and walks those of an NFT; by_nft_owner lists them by owner, in the order their
    * objects were created. A copy missing from both is still with its creator.
    */
  typedef multi_index_container<
    nft_edition_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< nft_edition_object, nft_edition_object::id_type, &nft_edition_object::get_id > >,
      ordered_unique< tag< by_nft_edition >,
        composite_key< nft_edition_object,
          const_mem_fun< nft_edition_object, nft_id_type, &nft_edition_object::get_nft >,
          const_mem_fun< nft_edition_object, uint16_t, &nft_edition_object::get_edition >
        >
      >,
      ordered_unique< tag< by_nft_owner >,
        composite_key< nft_edition_object,
          const_mem_fun< nft_edition_object, account_id_type, &nft_edition_object::get_owner >,
          const_mem_fun< nft_edition_object, nft_edition_object::id_type, &nft_edition_object::get_id >
        >
      >
    >,
    multi_index_allocator< nft_edition_object >
  > nft_edition_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::nft_object, hive::chain::nft_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::nft_edition_object, hive::chain::nft_edition_index )
