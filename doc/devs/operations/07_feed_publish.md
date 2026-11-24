# feed\_publish\_operation, // 7

## 1. Description

This is an operation for witnesses. 

The witnesses publish the exchange rate between PIXA and PXS. Only the exchange rate published by the top 20 witnesses is used to define the exchange rate.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------------------------------------------- | ------- |
| publisher      | The witness                                                                        |         |
| exchange\_rate | How many PXS the 1 PIXA should costExample:"base":"0.345 PXS","quote":"1.000 PIXA" |         |

## 3. Authority

Active


## 4. Operation preconditions

You need:

\- the operation is allowed for the witnesses only.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the exchange rate is published, if the { publisher } is the top 20 witness, then the {exchange\_rate} is used to define blockchain exchange rate.

\- RC cost is paid by { publisher }.
