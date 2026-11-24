# claim\_reward\_balance\_operation, // 39

## 1. Description

An operation claim\_reward\_balance\_operation is used to transfer previously collected author and/or curation rewards from sub balance for the reward to regular balances. Rewards expressed in PIXA and PXS are transferred to liquid balances, rewards in PXP increase vesting balance. When claimed, PXP rewards are immediately active towards governance voting power (compare with transfer\_to\_vesting\_operation)


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------- | ------- |
| account        | Account name                                                  |         |
| reward\_pixa   | The amount of PIXA reward to be transferred to liquid balance |         |
| reward\_pxs    | The amount of PXS reward to be transferred to liquid balance  |         |
| reward\_vests  | The amount of PXP reward to be transferred to vesting balance  |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- the reward (author or curator)  should be calculated.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the reward is transferred to the user balance – the balance is increased by the transferred amount ( in PIXA, PXS or PXP).

\- RC cost is paid by { account }.
