// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "NetconfRpcTestFixture.h"

#include "Common.h"

#include <limits>

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace {
// Short timeout so that Call Home tests move quickly
const uint32_t kNetconfRpcTimeoutSec = 1;
} // namespace

TEST_F(NetconfRpcTest, CorrectOk) {
  // Perform a NETCONF RPC which should give back OK
  std::string request =
      "<discard-changes xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\"/>";
  std::optional<mpclient::NetconfRpcResponse> response =
      client_.netconfRpc(sessionId_, request, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->status(), mpclient::NetconfRpcResponse::SUCCESS);
  ASSERT_TRUE(response->has_returntype());
  ASSERT_EQ(response->returntype(), mpclient::NetconfRpcResponse::OK);
  ASSERT_FALSE(response->has_message());
}

TEST_F(NetconfRpcTest, CorrectData) {
  // Perform a NETCONF RPC which should give back actual data
  std::string request = loadFile("tests/cases/01_get-config_request.xml");
  std::string actualResponse =
      loadFile("tests/cases/01_get-config_response.xml");
  std::optional<mpclient::NetconfRpcResponse> response =
      client_.netconfRpc(sessionId_, request, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->status(), mpclient::NetconfRpcResponse::SUCCESS);
  ASSERT_TRUE(response->has_returntype());
  ASSERT_EQ(response->returntype(), mpclient::NetconfRpcResponse::DATA);
  ASSERT_TRUE(response->has_message());
  ASSERT_EQ(response->message(), actualResponse);
}

TEST_F(NetconfRpcTest, CorrectDataError) {
  // Perform an RPC which gives an error (edit-config touches bogus fields)
  std::string request = loadFile("tests/cases/02_edit-config_request.xml");
  std::string actualResponse =
      loadFile("tests/cases/02_edit-config_response.xml");
  std::optional<mpclient::NetconfRpcResponse> response =
      client_.netconfRpc(sessionId_, request, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->status(), mpclient::NetconfRpcResponse::SUCCESS);
  ASSERT_TRUE(response->has_returntype());
  ASSERT_EQ(response->returntype(), mpclient::NetconfRpcResponse::RPC_ERROR);
  ASSERT_TRUE(response->has_message());
  ASSERT_EQ(response->message(), actualResponse);
}

TEST_F(NetconfRpcTest, CorrectError) {
  // NETCONF RPC does not get executed because the XML has no namespace
  std::string request = "<discard-changes/>";
  std::optional<mpclient::NetconfRpcResponse> response =
      client_.netconfRpc(sessionId_, request, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->status(), mpclient::NetconfRpcResponse::ERROR);
  ASSERT_FALSE(response->has_returntype());
  ASSERT_FALSE(response->has_message());
}

TEST_F(NetconfRpcTest, BadSessionId) {
  // Try to call the RPC on a (probably) invalid session
  std::string request = loadFile("tests/cases/01_get-config_request.xml");
  std::optional<mpclient::NetconfRpcResponse> response =
      client_.netconfRpc(1000000, request, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->status(), mpclient::NetconfRpcResponse::INVALID_SESSION);
  ASSERT_FALSE(response->has_returntype());
  ASSERT_FALSE(response->has_message());
}

TEST_F(NetconfRpcTest, MP_CONF_001_ModifyParameterInRunningDatastore) {
  // Step 1: Lock running datastore
  std::string lockRequest =
      "<lock xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
      "<target><running/></target>"
      "</lock>";
  auto lockResponse = client_.netconfRpc(sessionId_, lockRequest, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(lockResponse.has_value());
  ASSERT_EQ(lockResponse->status(), mpclient::NetconfRpcResponse::SUCCESS);

  // Step 2: Edit-config with known supported models
  std::string editConfigRequest =
      "<edit-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
      "<target><running/></target>"
      "<config>"
      "<interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
      "<interface>"
      "<name>eth0</name>"
      "<description>Updated description for eth0</description>"
      "<enabled>true</enabled>"
      "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>"
      "<l2-mtu xmlns=\"urn:o-ran:interfaces:1.0\">9100</l2-mtu>"
      "</interface>"
      "</interfaces>"
      "<users xmlns=\"urn:o-ran:user-mgmt:1.0\">"
      "<user>"
      "<name>USER</name>"
      "<account-type>PASSWORD</account-type>"
      "<enabled>true</enabled>"
      "</user>"
      "</users>"
      "</config>"
      "</edit-config>";
  auto editResponse = client_.netconfRpc(sessionId_, editConfigRequest, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(editResponse.has_value());
  // LOG(INFO) << "Edit-config response:\n" << editResponse->message();
  ASSERT_EQ(editResponse->status(), mpclient::NetconfRpcResponse::SUCCESS);

  // Step 3: Unlock running datastore
  std::string unlockRequest =
      "<unlock xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
      "<target><running/></target>"
      "</unlock>";
  auto unlockResponse = client_.netconfRpc(sessionId_, unlockRequest, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(unlockResponse.has_value());

  // Step 4: Get-config to verify
  std::string getConfigRequest =
      "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\"><source><running/></source></get-config>";
  // std::string getConfigRequest = loadFile("tests/cases/01_get-config_request.xml");
  auto getResponse = client_.netconfRpc(sessionId_, getConfigRequest, kNetconfRpcTimeoutSec);
  ASSERT_TRUE(getResponse.has_value());
  std::string payload = getResponse->message();
  // LOG(INFO) << "Get-config response:\n" << payload;

  // Step 5: Assert values are present in the running datastore
  EXPECT_NE(payload.find("<description>Updated description for eth0</description>"), std::string::npos)
      << "Interface description not found in running datastore";
  EXPECT_NE(
    payload.find("<l2-mtu xmlns=\"urn:o-ran:interfaces:1.0\">9100</l2-mtu>"),
    std::string::npos)
    << "MTU not updated in running datastore";

  EXPECT_NE(payload.find("<name>USER</name>"), std::string::npos)
      << "USER account not found in running datastore";
}


TEST_F(NetconfRpcTest, MP_CONF_002_LockDeniedForSecondClient) {
    auto logResponse = [](const std::string& title,
                          const std::optional<mpclient::NetconfRpcResponse>& resp) {
        LOG(INFO) << "\n=== " << title << " ===";
        if (!resp) {
            LOG(INFO) << "No response received.";
            return;
        }
        LOG(INFO) << "status(): " << resp->status();
        LOG(INFO) << "returntype(): "
                  << (resp->has_returntype() ? std::to_string(resp->returntype()) : "n/a");
        LOG(INFO) << "message():\n"
                  << (resp->has_message() ? resp->message() : "(no message)");
    };

    auto rpc = [&](int32_t sessionId, const std::string& xml, const std::string& title) {
        auto resp = client_.netconfRpc(sessionId, xml, kNetconfRpcTimeoutSec);
        logResponse(title, resp);
        return resp;
    };

    // ---------- Step 1: Client A locks running datastore ----------
    const std::string lockRequest =
        "<lock xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<target><running/></target>"
        "</lock>";
    auto lockResp = rpc(sessionId_, lockRequest, "Client A lock");
    ASSERT_TRUE(lockResp);
    ASSERT_EQ(lockResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_TRUE(lockResp->has_returntype());
    ASSERT_EQ(lockResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 2: Client B edit-config (should fail with lock-denied) ----------
    const std::string editConfigRequestB =
        "<edit-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<target><running/></target>"
        "<config>"
          "<interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
            "<interface>"
              "<name>eth0</name>"
              "<description>Updated description for eth0</description>"
              "<enabled>true</enabled>"
              "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">"
                "ianaift:ethernetCsmacd"
              "</type>"
              "<l2-mtu xmlns=\"urn:o-ran:interfaces:1.0\">9100</l2-mtu>"
            "</interface>"
          "</interfaces>"
          "<users xmlns=\"urn:o-ran:user-mgmt:1.0\">"
            "<user>"
              "<name>USER_B</name>"
              "<account-type>PASSWORD</account-type>"
              "<enabled>true</enabled>"
            "</user>"
          "</users>"
        "</config>"
        "</edit-config>";

    auto editRespB = rpc(sessionIdB_, editConfigRequestB, "Client B edit-config while locked");
    ASSERT_TRUE(editRespB);
    ASSERT_EQ(editRespB->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_TRUE(editRespB->has_returntype());
    ASSERT_EQ(editRespB->returntype(), mpclient::NetconfRpcResponse::RPC_ERROR);
    ASSERT_TRUE(editRespB->has_message());
    EXPECT_NE(editRespB->message().find("<error-tag>lock-denied</error-tag>"), std::string::npos)
        << "Expected lock-denied error-tag but got:\n" << editRespB->message();

    // ---------- Step 3: Client A unlocks ----------
    const std::string unlockRequest =
        "<unlock xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<target><running/></target>"
        "</unlock>";
    auto unlockResp = rpc(sessionId_, unlockRequest, "Client A unlock");
    ASSERT_TRUE(unlockResp);
    ASSERT_EQ(unlockResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_TRUE(unlockResp->has_returntype());
    ASSERT_EQ(unlockResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 4: Get-config to verify datastore ----------
    const std::string getConfigRequest =
        "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<source><running/></source>"
        "</get-config>";
    auto getResp = rpc(sessionId_, getConfigRequest, "Get-config after unlock");
    ASSERT_TRUE(getResp);
    ASSERT_EQ(getResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_TRUE(getResp->has_message());

    const std::string payload = getResp->message();
    if (payload.find("<description>Blocked update by client B</description>") != std::string::npos) {
        LOG(INFO) << "Unexpected blocked update description found.";
    } else {
        LOG(INFO) << "Blocked update description not present (expected).";
    }
}

TEST_F(NetconfRpcTest, MP_UPLANE_001_CreateConfigureActivateTxCarrier_NoLock) {
    auto logResponse = [](const std::string& title,
                          const std::optional<mpclient::NetconfRpcResponse>& resp) {
        LOG(INFO) << "\n=== " << title << " ===";
        if (!resp) {
            LOG(INFO) << "No response received.";
            return;
        }
        LOG(INFO) << "status(): " << resp->status();
        LOG(INFO) << "returntype(): "
                  << (resp->has_returntype() ? std::to_string(resp->returntype()) : "n/a");
        LOG(INFO) << "message():\n"
                  << (resp->has_message() ? resp->message() : "(no message)");
    };

    auto rpc = [&](int32_t sessionId, const std::string& xml, const std::string& title) {
        auto resp = client_.netconfRpc(sessionId, xml, kNetconfRpcTimeoutSec);
        logResponse(title, resp);
        return resp;
    };

    // ---------- Step 1: Create processing-element ----------
    const std::string createPE = R"(
        <edit-config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <target><running/></target>
          <config>
            <processing-elements xmlns="urn:o-ran:processing-element:1.0">
              <ru-elements>
                <name>PE_1</name>
                <transport-flow>
                  <interface-name>eth0_cu</interface-name>
                  <eth-flow>
                    <ru-mac-address>00:11:22:33:44:88</ru-mac-address>
                    <vlan-id>4000</vlan-id>
                    <o-du-mac-address>00:AA:BB:CC:DD:11</o-du-mac-address>
                  </eth-flow>
                </transport-flow>
              </ru-elements>
            </processing-elements>
          </config>
        </edit-config>
    )";
    auto createPEResp = rpc(sessionId_, createPE, "Create processing-element");
    ASSERT_TRUE(createPEResp);
    ASSERT_EQ(createPEResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_EQ(createPEResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 2: Create low-level-tx-endpoint ----------
    const std::string createTxEp = R"(
        <edit-config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <target><running/></target>
          <config>
            <user-plane-configuration xmlns="urn:o-ran:uplane-conf:1.0">
              <low-level-tx-endpoints>
                <low-level-tx-endpoint>
                  <name>TXEP_1</name>
                  <frame-structure>0</frame-structure>
                  <cp-type>NORMAL</cp-type>
                </low-level-tx-endpoint>
              </low-level-tx-endpoints>
            </user-plane-configuration>
          </config>
        </edit-config>
    )";
    auto createTxEpResp = rpc(sessionId_, createTxEp, "Create low-level-tx-endpoint");
    ASSERT_TRUE(createTxEpResp);
    ASSERT_EQ(createTxEpResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_EQ(createTxEpResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 3: Create new tx-array-carrier ----------
    const std::string createCarrier = R"(
        <edit-config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <target><running/></target>
          <config>
            <user-plane-configuration xmlns="urn:o-ran:uplane-conf:1.0">
              <tx-array-carriers>
                <tx-array-carrier>
                  <name>TXCARR_001</name>
                  <center-of-channel-bandwidth>3700000000</center-of-channel-bandwidth>
                  <channel-bandwidth>100000000</channel-bandwidth>
                  <active>INACTIVE</active>
                </tx-array-carrier>
              </tx-array-carriers>
            </user-plane-configuration>
          </config>
        </edit-config>
    )";
    auto createCarrierResp = rpc(sessionId_, createCarrier, "Create tx-array-carrier");
    ASSERT_TRUE(createCarrierResp);
    ASSERT_EQ(createCarrierResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_EQ(createCarrierResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 4: Create low-level-tx-link association ----------
    const std::string createTxLink = R"(
        <edit-config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <target><running/></target>
          <config>
            <user-plane-configuration xmlns="urn:o-ran:uplane-conf:1.0">
              <low-level-tx-links>
                <low-level-tx-link>
                  <name>TXLINK_001</name>
                  <processing-element>PE_1</processing-element>
                  <tx-array-carrier>TXCARR_001</tx-array-carrier>
                  <low-level-tx-endpoint>TXEP_1</low-level-tx-endpoint>
                </low-level-tx-link>
              </low-level-tx-links>
            </user-plane-configuration>
          </config>
        </edit-config>
    )";
    auto createTxLinkResp = rpc(sessionId_, createTxLink, "Create low-level-tx-link");
    ASSERT_TRUE(createTxLinkResp);
    ASSERT_EQ(createTxLinkResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_EQ(createTxLinkResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 5: Activate tx-array-carrier ----------
    const std::string activateCarrier = R"(
        <edit-config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <target><running/></target>
          <config>
            <user-plane-configuration xmlns="urn:o-ran:uplane-conf:1.0">
              <tx-array-carriers>
                <tx-array-carrier>
                  <name>TXCARR_001</name>
                  <active>ACTIVE</active>
                </tx-array-carrier>
              </tx-array-carriers>
            </user-plane-configuration>
          </config>
        </edit-config>
    )";
    auto activateCarrierResp = rpc(sessionId_, activateCarrier, "Activate tx-array-carrier");
    ASSERT_TRUE(activateCarrierResp);
    ASSERT_EQ(activateCarrierResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_EQ(activateCarrierResp->returntype(), mpclient::NetconfRpcResponse::OK);

    // ---------- Step 6: Verify carrier state ----------
    const std::string getCarrierState =
        R"(<get xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
             <filter>
               <user-plane-configuration xmlns="urn:o-ran:uplane-conf:1.0">
                 <tx-array-carriers>
                   <tx-array-carrier>
                     <name>TXCARR_001</name>
                   </tx-array-carriers>
               </user-plane-configuration>
             </filter>
           </get>)";
    auto getStateResp = rpc(sessionId_, getCarrierState, "Get carrier state");
    ASSERT_TRUE(getStateResp);
    ASSERT_EQ(getStateResp->status(), mpclient::NetconfRpcResponse::SUCCESS);
    ASSERT_TRUE(getStateResp->has_message());
    EXPECT_NE(getStateResp->message().find("<state>READY</state>"), std::string::npos)
        << "Expected carrier state READY, got:\n" << getStateResp->message();
}
