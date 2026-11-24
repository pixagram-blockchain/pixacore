# NFT - nft\_operation, // 37

## 1. Description

The operation of the native NFT feature (hardfork 32). An NFT is a root post that its author minted;
it is identified by the post, `author/permlink`, and exists in 1 to 10,000 copies (editions)
numbered from 1. Each copy has exactly one owner. The read APIs write `@author/permlink/0` for the
NFT and `@author/permlink/N` for copy N. Prices, royalties and the mint fee are in PXS.

The **reserve** is the copies that never left the author and have no offer of their own. Edition 0
names it: the author lists edition 0 to offer the whole reserve at one price, unlists it to withdraw
that offer, and a buyer of edition 0 gets the lowest-numbered copy still in the reserve. A copy the
author offers on its own leaves the reserve; when he withdraws that offer, it goes back.

The operation carries one action. `account` is the actor of the action.

| Action | Actor | What it does |
| ------ | ----- | ------------ |
| nft\_mint | author of the post | Mints the post with 1 to 10,000 copies and pays 1.000 PXS per copy, burned. The author owns every copy. Royalty and license are fixed for good, and the post can no longer be edited or deleted. |
| nft\_list | owner | Offers the copy at a fixed price in PXS, or replaces the offer. Edition 0: the author offers the reserve. |
| nft\_unlist | owner | Withdraws the offer. Edition 0: the author withdraws the offer of the reserve. |
| nft\_buy | buyer | Takes the offer: pays the price, the author receives the royalty unless he is the seller, the seller receives the rest; the buyer becomes the owner. Edition 0: buys the next copy of the reserve, at the author's price, all of it paid to the author. |
| nft\_transfer | owner | Gives the copy to another account without payment, so without royalty (a gift). Ends any offer. Giving it to `null` burns it. |

It took the number of `reset_account_operation`, a placeholder no block ever contained.
A purchase generates the virtual operations `nft_sold_operation` (93) and, when a royalty is paid,
`nft_royalty_operation` (94).

## 2.  Parameters

| Parameter name | Description | Example |
| -------------- | ----------- | ------- |
| account        | The actor: minter, owner or buyer | alice |
| action         | One of the actions below, as `{"type": ..., "value": {...}}` | |
| extensions     | Not currently used | |

nft\_mint

| Parameter name | Description | Example |
| -------------- | ----------- | ------- |
| permlink       | Permlink of a root post of {account} | my-artwork |
| editions       | Number of copies, 1 to 10,000 (`PIXA_NFT_MAX_EDITIONS`) | 5 |
| royalty        | Basis points of each later sale price paid to the author, 0 to 2500 | 1000 |
| license        | The full text of the license terms, UTF-8, at most 73,728 bytes (72 KiB, `PIXA_NFT_MAX_LICENSE_LENGTH`) | Licence « tous droits réservés » … |
| fee            | {editions} × 1.000 PXS (`PIXA_NFT_MINT_FEE_PER_EDITION`); `database_api.get_nft_params` returns the fee per copy | 5.000 PXS |

nft\_list, nft\_unlist, nft\_buy, nft\_transfer

| Parameter name | Description | Example |
| -------------- | ----------- | ------- |
| author         | Author of the minted post | alice |
| permlink       | Permlink of the minted post | my-artwork |
| edition        | Number of the copy, 1 to 10,000. 0 is the reserve, in nft\_list, nft\_unlist and nft\_buy | 3 |
| price          | nft\_list: asking price, in PXS. nft\_buy: has to equal the asking price | 50.000 PXS |
| expiration     | nft\_list: the offer cannot be taken at or after this time. Leave it out for an offer that never expires: the node then sets 2106-02-07T06:28:15, which it writes in what it returns but refuses in JSON it receives, as it refuses any time after 2038-01-19T03:14:07 (fc's ISO time parser stops at 2^31 - 1 seconds) | 2027-02-01T00:00:00 |
| to             | nft\_transfer: the account that becomes the owner; `null` burns the copy | bob |

A mint with the longest license fits in one transaction: Pixa transactions can be 128 KiB.

## 3.  Authority

Active

## 4.  Operation preconditions

\- hardfork 32 is active.

\- nft\_mint: the post exists, is a root post of {account} and was not minted before; {editions} is
1 to 10,000; {royalty} is at most 2500; {license} is UTF-8 and at most 72 KiB; {fee} is exactly
{editions} × 1.000 PXS, and {account} has it in liquid PXS.

\- nft\_list, nft\_unlist, nft\_transfer: {account} owns the copy; a copy that was never offered nor
moved is its author's. An offer needs a positive price in PXS and an expiration in the future
(the operation format also takes PIXA, which consensus refuses). The receiver of a transfer exists
and is not the treasury or one of the restricted accounts; it can be `null`.

\- nft\_buy: the copy is offered and the offer has not expired; {price} equals the asking price;
{account} is not the owner and has the price in liquid PXS.

\- Edition 0 (the reserve): only the author lists or unlists it. Listing it needs at least one copy in
the reserve, a price in PXS and an expiration in the future; unlisting it needs a recorded offer. A buy
needs the reserve offer, unexpired, and a copy in the reserve; {price} equals the author's price and
{account} is not the author. Edition 0 cannot be transferred.

## 5.  Impacted state

\- nft\_mint: {fee} is moved from {account} to `null`, where it is burned at the end of the block,
and the PXS supply goes down by it. One `nft_object` is created, whatever {editions}; it keeps the
SHA-256 and the length of {license}, not the text, which stays in this operation in the block log.
The copies get no object yet: each is its author's, acquired at mint time, with no offer.

\- nft\_list / nft\_unlist: the asking price and expiration of the copy are set or cleared. The first
offer of a copy creates its `nft_edition_object` and takes the copy out of the reserve. Withdrawing the
offer of a copy that never left its author removes that object: the copy is back in the reserve.
Edition 0 sets or clears the reserve offer kept on the `nft_object`; it creates no copy object.

\- nft\_buy: the price is moved from the buyer; `price * royalty / 10000`, rounded down to 0.001 PXS,
goes to the author unless the author is the seller; the rest goes to the seller. The owner of the
copy changes and its offer is cleared.

\- nft\_buy of edition 0: the price is moved from the buyer to the author, with no royalty. The lowest-
numbered copy of the reserve gets its `nft_edition_object`, owned by the buyer; `nft_sold_operation`
names that copy. The reserve offer ends by itself when the last copy of the reserve is gone.

\- nft\_transfer: the owner of the copy changes and its offer is cleared; the first move of a copy
creates its `nft_edition_object`. A copy given to `null` stays there for good: it keeps its number
and still counts in {editions}, and nobody can sign for it.

The `nft_object` is never removed; an `nft_edition_object` only as described for nft\_unlist. The chain
holds no funds for an NFT at any time.

## 6.  Example

```json
{
  "type": "nft_operation",
  "value": {
    "account": "alice",
    "action": {
      "type": "nft_mint",
      "value": {
        "permlink": "my-artwork",
        "editions": 5,
        "royalty": 1000,
        "license": "Licence « tous droits réservés »",
        "fee": {"amount": "5000", "precision": 3, "nai": "@@000000013"},
        "extensions": []
      }
    },
    "extensions": []
  }
}
```

To sell every copy still with the author at one price, list edition 0 once:

```json
{"type": "nft_list", "value": {"author": "alice", "permlink": "my-artwork", "edition": 0,
  "price": {"amount": "5000", "precision": 3, "nai": "@@000000013"}, "extensions": []}}
```

(no `expiration`: the offer never expires), and each buyer sends `nft_buy` with edition 0 and that price. To give some copies their own price, list
them one by one: they leave the reserve while their offer stands.
