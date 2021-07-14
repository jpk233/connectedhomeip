/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *      This file defines the CHIP CASE Session object that provides
 *      APIs for constructing a secure session using a certificate from the device's
 *      operational credentials.
 */

#pragma once

#include <credentials/CHIPCert.h>
#include <credentials/CHIPOperationalCredentials.h>
#include <crypto/CHIPCryptoPAL.h>
#if CHIP_CRYPTO_HSM
#include <crypto/hsm/CHIPCryptoPALHsm.h>
#endif
#include <messaging/ExchangeContext.h>
#include <messaging/ExchangeDelegate.h>
#include <protocols/secure_channel/Constants.h>
#include <protocols/secure_channel/SessionEstablishmentDelegate.h>
#include <protocols/secure_channel/SessionEstablishmentExchangeDispatch.h>
#include <support/Base64.h>
#include <system/SystemPacketBuffer.h>
#include <transport/PairingSession.h>
#include <transport/PeerConnectionState.h>
#include <transport/SecureSession.h>
#include <transport/raw/MessageHeader.h>
#include <transport/raw/PeerAddress.h>

namespace chip {

constexpr uint16_t kAEADKeySize = 16;

constexpr uint16_t kSigmaParamRandomNumberSize = 32;
constexpr uint16_t kTrustedRootIdSize          = Credentials::kKeyIdentifierLength;
constexpr uint16_t kMaxTrustedRootIds          = 5;

constexpr uint16_t kIPKSize = 16;

#ifdef ENABLE_HSM_CASE_EPHERMAL_KEY
#define CASE_EPHEMERAL_KEY 0xCA5EECD0
#endif

struct CASESessionSerialized;

struct CASESessionSerializable
{
    uint16_t mSharedSecretLen;
    uint8_t mSharedSecret[Crypto::kMax_ECDH_Secret_Length];
    uint16_t mMessageDigestLen;
    uint8_t mMessageDigest[Crypto::kSHA256_Hash_Length];
    uint8_t mPairingComplete;
    NodeId mPeerNodeId;
    uint16_t mLocalKeyId;
    uint16_t mPeerKeyId;
};

class DLL_EXPORT CASESession : public Messaging::ExchangeDelegate, public PairingSession
{
public:
    CASESession();
    CASESession(CASESession &&)      = default;
    CASESession(const CASESession &) = default;
    CASESession & operator=(const CASESession &) = default;
    CASESession & operator=(CASESession &&) = default;

    virtual ~CASESession();

    /**
     * @brief
     *   Initialize using operational credentials code and wait for session establishment requests.
     *
     * @param operationalCredentialSet      CHIP Certificate Set used to store the chain root of trust an validate peer node
     *                                      certificates
     * @param myKeyId                       Key ID to be assigned to the secure session on the peer node
     * @param delegate                      Callback object
     *
     * @return CHIP_ERROR     The result of initialization
     */
    CHIP_ERROR ListenForSessionEstablishment(Credentials::OperationalCredentialSet * operationalCredentialSet, uint16_t myKeyId,
                                             SessionEstablishmentDelegate * delegate);

    /**
     * @brief
     *   Create and send session establishment request using device's operational credentials.
     *
     * @param peerAddress                   Address of peer with which to establish a session.
     * @param operationalCredentialSet      CHIP Certificate Set used to store the chain root of trust an validate peer node
     *                                      certificates
     * @param opCredSetIndex                Index value used to choose the chain root of trust for establishing a session. Retrieve
     *                                      this index value from an operationalCredentialSet's entry that matches the device's
     *                                      operational credentials
     * @param peerNodeId                    Node id of the peer node
     * @param peerFabricId                  Fabric id of the peer node
     * @param myKeyId                       Key ID to be assigned to the secure session on the peer node
     * @param exchangeCtxt                  The exchange context to send and receive messages with the peer
     * @param delegate                      Callback object
     *
     * @return CHIP_ERROR      The result of initialization
     */
    CHIP_ERROR EstablishSession(const Transport::PeerAddress peerAddress,
                                Credentials::OperationalCredentialSet * operationalCredentialSet, uint8_t opCredSetIndex,
                                NodeId peerNodeId, FabricId peerFabricId, uint16_t myKeyId,
                                Messaging::ExchangeContext * exchangeCtxt, SessionEstablishmentDelegate * delegate);

    /**
     * @brief
     *   Derive a secure session from the established session. The API will return error
     *   if called before session is established.
     *
     * @param session     Reference to the secure session that will be
     *                    initialized once session establishment is complete
     * @param role        Role of the new session (initiator or responder)
     * @return CHIP_ERROR The result of session derivation
     */
    virtual CHIP_ERROR DeriveSecureSession(SecureSession & session, SecureSession::SessionRole role) override;

    /**
     * @brief
     *  Return the associated secure session peer NodeId
     *
     * @return NodeId The associated peer NodeId
     */
    NodeId GetPeerNodeId() const { return mConnectionState.GetPeerNodeId(); }

    /**
     * @brief
     *  Return the associated peer key id
     *
     * @return uint16_t The associated peer key id
     */
    uint16_t GetPeerKeyId() override { return mConnectionState.GetPeerKeyID(); }

    /**
     * @brief
     *  Return the associated local key id
     *
     * @return uint16_t The assocated local key id
     */
    uint16_t GetLocalKeyId() override { return mConnectionState.GetLocalKeyID(); }

    const char * GetI2RSessionInfo() const override { return "Sigma I2R Key"; }

    const char * GetR2ISessionInfo() const override { return "Sigma R2I Key"; }

    Transport::PeerConnectionState & PeerConnection() { return mConnectionState; }

    /**
     * @brief Serialize the Pairing Session to a string.
     **/
    CHIP_ERROR Serialize(CASESessionSerialized & output);

    /**
     * @brief Deserialize the Pairing Session from the string.
     **/
    CHIP_ERROR Deserialize(CASESessionSerialized & input);

    /**
     * @brief Serialize the CASESession to the given serializable data structure for secure pairing
     **/
    CHIP_ERROR ToSerializable(CASESessionSerializable & output);

    /**
     * @brief Reconstruct secure pairing class from the serializable data structure.
     **/
    CHIP_ERROR FromSerializable(const CASESessionSerializable & output);

    SessionEstablishmentExchangeDispatch & MessageDispatch() { return mMessageDispatch; }

    //// ExchangeDelegate Implementation ////
    CHIP_ERROR OnMessageReceived(Messaging::ExchangeContext * ec, const PacketHeader & packetHeader,
                                 const PayloadHeader & payloadHeader, System::PacketBufferHandle && payload) override;
    void OnResponseTimeout(Messaging::ExchangeContext * ec) override;
    Messaging::ExchangeMessageDispatch * GetMessageDispatch(Messaging::ReliableMessageMgr * rmMgr,
                                                            SecureSessionMgr * sessionMgr) override
    {
        return &mMessageDispatch;
    }

    /** @brief This function zeroes out and resets the memory used by the object.
     **/
    void Clear();

private:
    enum SigmaErrorType : uint8_t
    {
        kInvalidSignature     = 0x04,
        kInvalidResumptionTag = 0x05,
        kUnsupportedVersion   = 0x06,
        kUnexpected           = 0xff,
    };

    CHIP_ERROR Init(Credentials::OperationalCredentialSet * operationalCredentialSet, uint16_t myKeyId,
                    SessionEstablishmentDelegate * delegate);

    CHIP_ERROR SendSigmaR1();
    CHIP_ERROR HandleSigmaR1_and_SendSigmaR2(System::PacketBufferHandle & msg);
    CHIP_ERROR HandleSigmaR1(System::PacketBufferHandle & msg);
    CHIP_ERROR SendSigmaR2();
    CHIP_ERROR HandleSigmaR2_and_SendSigmaR3(System::PacketBufferHandle & msg);
    CHIP_ERROR HandleSigmaR2(System::PacketBufferHandle & msg);
    CHIP_ERROR SendSigmaR3();
    CHIP_ERROR HandleSigmaR3(System::PacketBufferHandle & msg);

    CHIP_ERROR SendSigmaR1Resume();
    CHIP_ERROR HandleSigmaR1Resume_and_SendSigmaR2Resume(const PacketHeader & header, const System::PacketBufferHandle & msg);

    CHIP_ERROR GenerateDestinationID(const ByteSpan & random, const Credentials::CertificateKeyId * trustedRootId, NodeId nodeId,
                                     FabricId fabricId, MutableByteSpan & destinationId);
    CHIP_ERROR FindDestinationIdCandidate(const ByteSpan & destinationId, const ByteSpan & initiatorRandom);
    CHIP_ERROR ConstructSaltSigmaR2(const ByteSpan & rand, const Crypto::P256PublicKey & pubkey, const ByteSpan & ipk,
                                    MutableByteSpan & salt);
    CHIP_ERROR Validate_and_RetrieveResponderID(const ByteSpan & responderOpCert, Crypto::P256PublicKey & responderID);
    CHIP_ERROR ConstructSaltSigmaR3(const ByteSpan & ipk, MutableByteSpan & salt);
    CHIP_ERROR ConstructTBS2Data(const ByteSpan & responderOpCert, uint8_t * tbsData, uint16_t & tbsDataLen);
    CHIP_ERROR ConstructTBS3Data(const ByteSpan & responderOpCert, uint8_t * tbsData, uint16_t & tbsDataLen);
    CHIP_ERROR RetrieveIPK(FabricId fabricId, MutableByteSpan & ipk);

    void SendErrorMsg(SigmaErrorType errorCode);

    // This function always returns an error. The error value corresponds to the error in the received message.
    // The returned error value helps top level message receiver/dispatcher to close the exchange context
    // in a more seamless manner.
    CHIP_ERROR HandleErrorMsg(const System::PacketBufferHandle & msg);

    void CloseExchange();

    // TODO: Remove this and replace with system method to retrieve current time
    CHIP_ERROR SetEffectiveTime(void);

    CHIP_ERROR ValidateReceivedMessage(Messaging::ExchangeContext * ec, const PacketHeader & packetHeader,
                                       const PayloadHeader & payloadHeader, System::PacketBufferHandle & msg);

    SessionEstablishmentDelegate * mDelegate = nullptr;

    Protocols::SecureChannel::MsgType mNextExpectedMsg = Protocols::SecureChannel::MsgType::CASE_SigmaErr;

    Crypto::Hash_SHA256_stream mCommissioningHash;
    Crypto::P256PublicKey mRemotePubKey;
#ifdef ENABLE_HSM_CASE_EPHERMAL_KEY
    Crypto::P256KeypairHSM mEphemeralKey;
#else
    Crypto::P256Keypair mEphemeralKey;
#endif
    Crypto::P256ECDHDerivedSecret mSharedSecret;
    Credentials::OperationalCredentialSet * mOpCredSet;
    Credentials::CertificateKeyId mTrustedRootId;
    Credentials::ValidationContext mValidContext;

    uint8_t mMessageDigest[Crypto::kSHA256_Hash_Length];
    uint8_t mIPK[kIPKSize];

    Messaging::ExchangeContext * mExchangeCtxt = nullptr;
    SessionEstablishmentExchangeDispatch mMessageDispatch;

    struct SigmaErrorMsg
    {
        SigmaErrorType error;
    };

protected:
    bool mPairingComplete = false;

    Transport::PeerConnectionState mConnectionState;
};

typedef struct CASESessionSerialized
{
    // Extra uint64_t to account for padding bytes (NULL termination, and some decoding overheads)
    uint8_t inner[BASE64_ENCODED_LEN(sizeof(CASESessionSerializable) + sizeof(uint64_t))];
} CASESessionSerialized;

} // namespace chip
