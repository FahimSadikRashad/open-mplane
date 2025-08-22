# Sprint 1 - Test Execution Overview

This sprint focused on validating core M-Plane functionalities of the O-RU, including startup & installation, software management, fault management, configuration, U-Plane handling, performance monitoring, and security. Below is the consolidated test case matrix with expected outputs, actual outputs, blockers, artifacts, and execution status.

| Test Case      | Name                                                   | Expected Output                     | Actual Output    | Artifacts   | Blockers   | Tried/Not Tried   |
|:---------------|:-------------------------------------------------------|:------------------------------------|:-----------------|:------------|:-----------|:------------------|
| MP_STARTUP_001 | O-RU establishes NETCONF session with O-DU (Call Home) | Successful session established      | Same as expected | None        | None       | Tried             |
| MP_STARTUP_002 | Failed Call Home retry after timer interval            | Retry attempts logged               | Not executed | None        | `re-call-home-no-ssh-timer` needed       | Not Tried             |
| MP_SW_001      | Successful software update cycle                       | New software slot active            | Not executed     | None        | `software-management` handler needed       | Not Tried         |
| MP_SW_002      | Installation fails on bad checksum                     | Install-event shows INTEGRITY_ERROR | Not executed     | None        | `software-management` handler needed       | Not Tried         |
| MP_FM_001      | Client receives alarm notifications                    | Alarms received & cleared           | Not executed | None        | `fault-managemet` handler needed        | Not Tried             |
| MP_CONF_001    | Modify parameter in running datastore                  | Parameter updated                   | Same as expected | None        | None       | Tried             |
| MP_CONF_002    | Reject edit-config from second client                  | Lock-denied error returned          | Server blocks access but don't send the rpc     | None        | None       | Tried         |
| MP_UPLANE_001  | Activate tx-array-carrier successfully                 | Carrier state READY                 | `tx-array-carrier` not found     | None        | Need knowledge about `YangConfig.xml`        | Tried         |
| MP_UPLANE_002  | Activation fails if sync=FREERUN                       | Edit-config fails                   | Not executed     | None        | Depends on  `MP_UPLANE_001`    | Not Tried         |
| MP_PM_001      | Performance measurement results uploaded               | CSV uploaded to sFTP                | Not executed     | None        | `software-management` handler needed       | Not Tried         |
| MP_SEC_001     | Sudo user creates fm-pm user                           | User created successfully           | Same as expected | None        | None       | Not Tried             |
| MP_SEC_002     | fm-pm user denied software mgmt RPC                    | Access denied error                 | Not executed     | None        | None       | Not Tried         |