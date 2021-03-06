#ifdef __RTP_CRYPTO__
#include <cassert>
#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/commit.hh"

#define ZRTP_COMMIT "Commit  "

uvg_rtp::zrtp_msg::commit::commit(zrtp_session_t& session)
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32] = { 0 };

    LOG_DEBUG("Create ZRTP Commit message!");

    frame_  = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_commit));
    rframe_ = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_commit));
    len_    = sizeof(zrtp_commit);
    rlen_   = sizeof(zrtp_commit);

    memset(frame_,  0, sizeof(zrtp_commit));
    memset(rframe_, 0, sizeof(zrtp_commit));

    zrtp_commit *msg = (zrtp_commit *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, ZRTP_COMMIT,                 8);
    memcpy(msg->zid,                 session.o_zid,              12); /* 96 bits */
    memcpy(msg->hash,                session.hash_ctx.o_hash[2], 32); /* 256 bits */
    memcpy(msg->hvi,                 session.hash_ctx.o_hvi,     32); /* 256 bits */

    msg->sas_type           = session.sas_type;
    msg->hash_algo          = session.hash_algo;
    msg->cipher_algo        = session.cipher_algo;
    msg->auth_tag_type      = session.auth_tag_type;
    msg->key_agreement_type = session.key_agreement_type;

    /* Calculate truncated HMAC-SHA256 for the Commit Message */
    auto hmac_sha256 = uvg_rtp::crypto::hmac::sha256(session.hash_ctx.o_hash[1], 32);

    hmac_sha256.update((uint8_t *)frame_, len_ - 8 - 4);
    hmac_sha256.final(mac_full);

    memcpy(&msg->mac, mac_full, sizeof(uint64_t));

    /* Calculate CRC32 for the whole ZRTP packet */
    uvg_rtp::crypto::crc32::get_crc32((uint8_t *)frame_, len_ - 4, &msg->crc);

    /* Finally make a copy of the message and save it for later use */
    session.l_msg.commit.first  = len_;
    session.l_msg.commit.second = (uvg_rtp::zrtp_msg::zrtp_commit *)new uint8_t[len_];
    memcpy(session.l_msg.commit.second, msg, len_);
}

uvg_rtp::zrtp_msg::commit::~commit()
{
    LOG_DEBUG("Freeing Commit message...");

    (void)uvg_rtp::frame::dealloc_frame(frame_);
    (void)uvg_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t uvg_rtp::zrtp_msg::commit::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes;
    WSABUF data_buf;

    data_buf.buf = (char *)frame_;
    data_buf.len = len_;

    if (WSASendTo(socket, &data_buf, 1, NULL, 0, (const struct sockaddr *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        return RTP_SEND_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t uvg_rtp::zrtp_msg::commit::parse_msg(uvg_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_commit *msg = (zrtp_commit *)rframe_;

    session.sas_type           = msg->sas_type;
    session.hash_algo          = msg->hash_algo;
    session.cipher_algo        = msg->cipher_algo;
    session.auth_tag_type      = msg->auth_tag_type;
    session.key_agreement_type = msg->key_agreement_type;

    memcpy(session.hash_ctx.r_hvi,     msg->hvi,  32);
    memcpy(&session.hash_ctx.r_mac[2], &msg->mac,  8);
    memcpy(session.hash_ctx.r_hash[2], msg->hash, 32);

    /* Finally make a copy of the message and save it for later use */
    session.r_msg.commit.first  = len;
    session.r_msg.commit.second = (uvg_rtp::zrtp_msg::zrtp_commit *)new uint8_t[len];
    memcpy(session.r_msg.commit.second, msg, len);

    return RTP_OK;
}
#endif
