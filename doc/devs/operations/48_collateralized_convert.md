# Collateralized convert - collateralized\_convert\_operation, // 48

## 1. Description

Similar to convert\_operation, this operation instructs the blockchain to convert PIXA to PXS.

The operation is performed after 3.5 days, but the owner gets PXS  immediately. The price risk is cushioned by extra PIXA (HIVE\_COLLATERAL\_RATIO = 200 % ). After actual conversion takes place the excess PIXA is returned to the owner.

The global parameters:

\- HIVE\_COLLATERALIZED\_CONVERSION\_DELAY, default mainnet value: 3.5 days   
\- HIVE\_COLLATERAL\_RATIO, default mainnet value: 200 %   
\- HIVE\_COLLATERALIZED\_CONVERSION\_FEE, default mainnet value: 5 %    


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------- | ------- |
| owner          | account\_name\_type                                         |         |
| requestid = 0; | The number is given by a user. Should be unique for a user. |         |
| amount         | Amount > 0, have to be in Hive                              |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in PIXA is equal or higher than the operation amount.

\- enough Resource Credit (RC)  to make an operation.


## 5.  Impacted state

After the operation:

\- immediately, the {owner} account balance in PIXA is reduced by the operation {amount}.

\- immediately, the first part of the {amount} is sent to collateral and the second part is exchanged on the minimal exchange rate from the last 3.5 days decreased by fee (5%), so  {owner} account balance in PXS is increased by the exchanged amount.

\- immediately, the virtual operation collateralized\_convert\_immediate\_conversion\_operation is generated.

\- 3.5 days after the operation, the PXS amount is recalculated using actual  median feed price with applied fee (5%). The difference between {amount} and recalculated amount increases the balance in PIXA.

\- when the conversion is executed (after 3.5 days), the virtual operation: fill\_collateralized\_convert\_request\_operation is generated. 

\- RC cost is paid by {owner} account.

**Example**

A user wants to convert PIXA to PXS. {amount} = 200 000 PIXA.

HIVE\_COLLATERAL\_RATIO = 200 %

FEE = 5%

Minimal feed price = every hour the median price of the feed price provided by the witnesses (top twenty) is calculated. The minimum of the median provided during the last 3.5 days is Minimal feed price.

Median feed price = every hour the median price of the feed price provided by the witnesses (top twenty) is calculated. The median of the median provided during the last  3,5 days is Median feed price.

Minimal feed price = 0,400

Median feed price = 0,407

Median feed price after 3.5 days =  0, 404

**Before an operation:**

**Balance PIXA**: 200 000 PIXA

**Balance PXS**: 0 PXS

**Immediately after the operation:**

- {amount} is divided into two parts (because HIVE\_COLLATERAL\_RATIO = 200 %), one is sent to collateral, second is exchanged

**Balance PIXA**: 200 000 PIXA - 200 000 = 0 PIXA

**Collateral**:  100 000 Hive

- Minimal feed price is used to exchange, the fee is applied to the exchange rate.

The minimal feed price: 0,400, so 400 PXS for 1000 PIXA.

The 5% fee is applied: 400 PXS for **1050** Hive

PXS amount: 100 000 \* 400 / 1050 = 38 095,2 PXS

**Balance PXS**: 0 + 38 095,2 PXS = 38 095,2 PXS

**3.5 days after the operation:**

- Current median feed price is used to exchange, the fee is applied to the exchange rate.

Median feed price =  0, 404,  so 404 PXS for 1000 PIXA.

The 5% fee is applied: 404 PXS for **1050** Hive

Hive amount: 38 095,2 \* 1050 / 404 = 99009,90

-  The calculated amount is deducted from {amount}

200 000 - 99009,90 = 100990,01 PIXA

**Balance PIXA**: 0+100990,01  = 100990,01 PIXA

**Collateral**:0 PIXA

**Balance PXS:** 38 095,2 PXS
