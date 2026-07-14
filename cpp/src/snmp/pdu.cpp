#include "snmp/pdu.hpp"

namespace wiresprite {

std::string SnmpMessage::encode() const {
    std::string pduContents;
    pduContents += ber::encode(ber::Value::integer(requestId));
    pduContents += ber::encode(ber::Value::integer(errorStatus));
    pduContents += ber::encode(ber::Value::integer(errorIndex));

    std::vector<ber::Value> varbindSeqs;
    varbindSeqs.reserve(varBinds.size());
    for (const auto& vb : varBinds) {
        varbindSeqs.push_back(ber::Value::sequence({ber::Value::objectIdentifier(vb.name), vb.value}));
    }
    pduContents += ber::encode(ber::Value::sequence(std::move(varbindSeqs)));

    std::string pduTlv = ber::encodeTlv(static_cast<uint8_t>(pduType), pduContents);

    std::string messageContents;
    messageContents += ber::encode(ber::Value::integer(static_cast<int32_t>(version)));
    messageContents += ber::encode(ber::Value::octetString(community));
    messageContents += pduTlv;

    return ber::encodeTlv(static_cast<uint8_t>(ber::Tag::Sequence), messageContents);
}

SnmpMessage SnmpMessage::decode(const std::string& buf) {
    size_t pos = 0;
    ber::RawTlv outer = ber::decodeTlv(buf, pos);
    if (outer.tag != static_cast<uint8_t>(ber::Tag::Sequence)) {
        throw ber::DecodeError("SnmpMessage::decode: expected top-level SEQUENCE");
    }

    size_t innerPos = 0;
    ber::Value versionVal = ber::decode(outer.contents, innerPos);
    ber::Value communityVal = ber::decode(outer.contents, innerPos);
    ber::RawTlv pduTlv = ber::decodeTlv(outer.contents, innerPos);

    SnmpMessage msg;
    msg.version = static_cast<SnmpVersion>(versionVal.asInt());
    msg.community = communityVal.asOctetString();
    msg.pduType = static_cast<PduType>(pduTlv.tag);

    size_t pduPos = 0;
    ber::Value requestIdVal = ber::decode(pduTlv.contents, pduPos);
    ber::Value errorStatusVal = ber::decode(pduTlv.contents, pduPos);
    ber::Value errorIndexVal = ber::decode(pduTlv.contents, pduPos);
    ber::Value varbindListVal = ber::decode(pduTlv.contents, pduPos);

    msg.requestId = static_cast<int32_t>(requestIdVal.asInt());
    msg.errorStatus = static_cast<int32_t>(errorStatusVal.asInt());
    msg.errorIndex = static_cast<int32_t>(errorIndexVal.asInt());

    for (const auto& vbSeq : varbindListVal.asSequence()) {
        const auto& fields = vbSeq.asSequence();
        if (fields.size() != 2) {
            throw ber::DecodeError("SnmpMessage::decode: VarBind must have exactly 2 fields");
        }
        VarBind vb;
        vb.name = fields[0].asOid();
        vb.value = fields[1];
        msg.varBinds.push_back(std::move(vb));
    }

    return msg;
}

} // namespace wiresprite
