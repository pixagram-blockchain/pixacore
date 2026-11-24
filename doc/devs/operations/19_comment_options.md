# comment\_options\_operation, // 19

## 1. Description

The operation comment\_options\_operation allows to set properties regarding payouts, rewards  or beneficiaries (using {extensions}) for comments.

If the operation: comment\_options\_operation is done by one of the frontends, it is usually in the same transaction with the operation: comment\_operation.

If a comment has received any votes, only the parameter {percent\_pxs} may be changed.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| author                   | Account name, the author of the comment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |         |
| permlink                 | The identifier of the comment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |         |
| max\_accepted\_payout    | The maximum value of payout in PXS.<br>Default value: max\_accepted\_payout = asset( 1000000000, PXS\_SYMBOL )<br>The allowed value should be less than the default value.<br>If max\_accepted\_payout = 0, then voters and authors will not receive the payout.                                                                                                                                                                                                                                                                       |         |
| percent\_pxs             | By default the author reward is paid 50% PXP and 50 % PXS. In some rare situations, instead of PXS, the PIXA may be paid.<br>percent\_pxs = HIVE\_100\_PERCENT means that 100 % of PXS part is paid in PXS.<br>A user may decide how many percent of PXS (from 50 %) they wants to receive in the PXS, the rest will be paid out in PXP.<br>Default value: percent\_pxs = HIVE\_100\_PERCENTThe allowed value should be less than the default value.<br>This is the only parameter that can be modified after the comment receives any vote. |         |
| allow\_votes             | The flag that allows to decide whether the comment may receive a vote.<br>Default value: allow\_votes = true.                                                                                                                                                                                                                                                                                                                                                                                                                  |         |
| allow\_curation\_rewards | The flag that allows to decide whether the voters for the comment should receive the curation rewards. Rewards return to the reward fund.<br>Default value: allow\_curation\_rewards = true.                                                                                                                                                                                                                                                                                                                                   |         |
| extensions               | It may contain the list of the beneficiaries, the accounts that should receive the author reward. The list consists of the account name and the weight of the shares in the author reward.<br>If the sum of the weights is less than 100%, the rest of the reward is received by the author.<br>It should be defined less than 128 accounts.<br>The allowed range of the weight is from 0 to 10000 (0 – 100%).<br>The beneficiaries should be listed in alphabetical order, no duplicates.                                                 |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- enough Resource Credit (RC)  to make an operation.

\- you have to be an author of the comment.

\- if a comment has received any votes, only  {percent\_pxs} may be changed.


## 5. Impacted state

After the operation:

\- the updated properties are applied.

\- RC cost is paid by { author }.

\- if the post or the comment receives the reward for the author, the beneficiary is set (using operation comment\_options\_operation)  and the beneficiary balance is increased, the virtual operation: comment\_benefactor\_reward\_operation is generated.
