#pragma once
#include <hive/protocol/hive_specialised_exceptions.hpp>
#include <hive/protocol/base.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/operation_util.hpp>

namespace hive { namespace protocol {

  /**
    * Native NFTs (hardfork 32).
    *
    * An NFT is a root post that its author minted. It is identified by the post, (author, permlink),
    * and exists in `editions` copies numbered from 1; (author, permlink, edition) names one copy, which
    * has exactly one owner at any time. The read APIs write them "@author/permlink/0" for the NFT and
    * "@author/permlink/N" for copy N. Everything that can be done with an NFT is one action carried by
    * nft_operation. New actions are appended to nft_action, which never moves an existing action or
    * any operation number.
    *
    * The reserve is the copies that never left the author and have no offer of their own. Edition 0
    * names it in nft_list, nft_unlist and nft_buy: the author offers the whole reserve at one price,
    * and each buyer of edition 0 gets the lowest-numbered copy still in it.
    *
    * Prices, royalties and the mint fee are in PXS.
    */

  /**
    * Mints the root post (account, permlink). The account signing the operation has to be the author.
    * All copies start owned by the author. Royalty and license are fixed here for the life of the NFT,
    * and from this point the post can no longer be edited or deleted. The fee is paid per copy.
    */
  struct nft_mint
  {
    string            permlink;
    /// Number of copies, 1 to PIXA_NFT_EDITIONS_LIMIT (10,000).
    /// Consensus accepts 1 to database::get_nft_max_editions().
    uint16_t          editions = 1;
    /// Share of each later sale price paid to the author, in basis points (500 = 5%).
    /// Consensus accepts 0 to database::get_nft_max_royalty().
    uint16_t          royalty = 0;
    /// Full text of the license terms. UTF-8, at most PIXA_NFT_MAX_LICENSE_LENGTH bytes (72 KiB).
    /// The text stays in this operation; the state keeps its SHA-256 and its length.
    string            license;
    /// Mint fee in PXS: database::get_nft_mint_fee( editions ), 1.000 PXS per copy, exactly. It is burned.
    asset             fee = asset( 0, PXS_SYMBOL );
    extensions_type   extensions;

    void validate() const;
  };

  /**
    * Offers a copy for sale at a fixed price, or replaces its current offer.
    * Only the owner can list. The copy stays with the owner until someone buys it.
    * Edition 0 offers the reserve: only the author can, while at least one copy is in it, and the offer
    * ends by itself when the last one is sold or leaves the reserve another way.
    */
  struct nft_list
  {
    account_name_type author;
    string            permlink;
    /// The copy, 1 to the number of copies; 0 for the reserve.
    uint16_t          edition = 1;
    /// Asking price. Consensus accepts PXS only (database::is_nft_price_symbol()).
    asset             price = asset( 0, PXS_SYMBOL );
    /// The offer cannot be taken at or after this time. The default never expires.
    time_point_sec    expiration = time_point_sec::maximum();
    extensions_type   extensions;

    void validate() const;
  };

  /**
    * Withdraws the offer made with nft_list. Only the owner can do it. Edition 0 withdraws the offer of
    * the reserve. A copy that never left the author goes back to the reserve.
    */
  struct nft_unlist
  {
    account_name_type author;
    string            permlink;
    /// The copy, 1 to the number of copies; 0 for the reserve.
    uint16_t          edition = 1;
    extensions_type   extensions;

    void validate() const;
  };

  /**
    * Buys a listed copy. `price` has to equal the asking price exactly, so a buyer never pays a price
    * changed after the transaction was signed. The buyer pays the price; the author receives the royalty
    * unless the author is the seller; the seller receives the rest; the buyer becomes the owner.
    * Edition 0 buys from the reserve at the author's price: the buyer gets the lowest-numbered copy still
    * in it, which nft_sold_operation names, and the author receives the whole price.
    */
  struct nft_buy
  {
    account_name_type author;
    string            permlink;
    /// The copy, 1 to the number of copies; 0 for the next copy of the reserve.
    uint16_t          edition = 1;
    asset             price = asset( 0, PXS_SYMBOL );
    extensions_type   extensions;

    void validate() const;
  };

  /**
    * Gives a copy to another account without payment, so without royalty. Only the owner can do it.
    * Ends any offer. Giving it to the null account burns it.
    */
  struct nft_transfer
  {
    account_name_type author;
    string            permlink;
    uint16_t          edition = 1;
    account_name_type to;
    extensions_type   extensions;

    void validate() const;
  };

  /** NOTE: do not change the order of the actions, only append: the position is the binary tag. */
  typedef fc::static_variant<
        nft_mint,     // 0
        nft_list,     // 1
        nft_unlist,   // 2
        nft_buy,      // 3
        nft_transfer  // 4
      > nft_action;

  /**
    * The one operation of the NFT feature. `account` is the actor of the action - the minter, the
    * owner listing, unlisting or transferring, or the buyer - and signs with the active authority.
    */
  struct nft_operation : public base_operation
  {
    account_name_type account;
    nft_action        action;
    extensions_type   extensions;

    void validate() const;
    void get_required_active_authorities( flat_set< account_name_type >& a ) const { a.insert( account ); }
  };

} } // hive::protocol

namespace fc
{
  using hive::protocol::nft_action;

  template<>
  struct serialization_functor< nft_action >
  {
    bool operator()( const fc::variant& v, nft_action& s ) const
    {
      return extended_serialization_functor< nft_action >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< nft_action >
  {
    template< typename T >
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< nft_action >().create( v, nft_action::tag< T >::value );
    }
  };
}

FC_REFLECT( hive::protocol::nft_mint, (permlink)(editions)(royalty)(license)(fee)(extensions) )
FC_REFLECT( hive::protocol::nft_list, (author)(permlink)(edition)(price)(expiration)(extensions) )
FC_REFLECT( hive::protocol::nft_unlist, (author)(permlink)(edition)(extensions) )
FC_REFLECT( hive::protocol::nft_buy, (author)(permlink)(edition)(price)(extensions) )
FC_REFLECT( hive::protocol::nft_transfer, (author)(permlink)(edition)(to)(extensions) )
FC_REFLECT_TYPENAME( hive::protocol::nft_action )
FC_REFLECT( hive::protocol::nft_operation, (account)(action)(extensions) )
