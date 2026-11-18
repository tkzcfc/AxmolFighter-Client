#include "doctest.h"

#include "mugen/core/reflect/SchemaHash.h"
#include "mugen/core/serialize/BinaryArchive.h"
#include "mugen/core/serialize/Hash.h"

#include <cstring>

using namespace mugen;

namespace
{
enum class TestKind : uint16_t
{
    Alpha = 7,
    Beta  = 9,
};

struct Nested
{
    int32_t id = 0;
    FixedVec2 position;
    MG_REFLECT(Nested, id, position)
};

struct ArchiveData
{
    bool enabled = false;
    TestKind kind = TestKind::Alpha;
    Fixed fixed;
    FixedVec3 direction;
    std::string name;
    std::vector<std::vector<int32_t>> matrix;
    std::array<uint16_t, 2> pair{};
    std::optional<Nested> nested;
    std::variant<int32_t, std::string> choice;
    std::map<int32_t, Nested> entries;
    MG_REFLECT(ArchiveData, enabled, kind, fixed, direction, name, matrix, pair, nested, choice, entries)
};

struct BaseData
{
    int32_t base = 0;
    MG_REFLECT(BaseData, base)
};

struct DerivedData : BaseData
{
    int32_t child = 0;
    MG_REFLECT_DERIVED(DerivedData, BaseData, child)
};

struct SchemaBefore
{
    int32_t id = 0;
    MG_REFLECT(SchemaBefore, id)
};

struct SchemaAfter
{
    int32_t id    = 0;
    int32_t extra = 0;
    MG_REFLECT(SchemaAfter, id, extra)
};

struct SchemaTwinOne
{
    int32_t id = 0;
    MG_REFLECT(SchemaTwinOne, id)
};

struct SchemaTwinTwo
{
    int32_t id = 0;
    MG_REFLECT(SchemaTwinTwo, id)
};

class ResizableBuffer : public ByteBuffer
{
public:
    using ByteBuffer::ByteBuffer;
    using ByteBuffer::resize;
};

ArchiveData makeData()
{
    ArchiveData value;
    value.enabled   = true;
    value.kind      = TestKind::Beta;
    value.fixed     = Fixed::ratio(3, 2);
    value.direction = {Fixed::fromInt(1), Fixed::fromInt(-2), Fixed::ratio(1, 4)};
    value.name      = "canonical";
    value.matrix    = {{1, 2}, {3, 4, 5}};
    value.pair      = {11, 12};
    value.nested    = Nested{42, {Fixed::fromInt(6), Fixed::fromInt(7)}};
    value.choice    = std::string("variant");
    value.entries.emplace(2, Nested{8, {Fixed::fromInt(9), Fixed::fromInt(10)}});
    value.entries.emplace(1, Nested{7, {Fixed::fromInt(4), Fixed::fromInt(5)}});
    return value;
}
}

TEST_CASE("Reflect archive round trip preserves canonical bytes")
{
    const ArchiveData source = makeData();
    ByteBuffer first;
    BinaryWriter firstWriter(first);
    CHECK(firstWriter.write(source));
    firstWriter.finish();

    ArchiveData decoded;
    BinaryReader reader(first);
    CHECK(reader.read(decoded));
    CHECK(reader.ok());

    ByteBuffer second;
    BinaryWriter secondWriter(second);
    CHECK(secondWriter.write(decoded));
    secondWriter.finish();
    REQUIRE(first.len() == second.len());
    CHECK(std::memcmp(first.data(), second.data(), first.len()) == 0);
}

TEST_CASE("Reflect derived visits base fields first")
{
    DerivedData value;
    value.base  = 0x01020304;
    value.child = 0x05060708;
    ByteBuffer buffer;
    BinaryWriter writer(buffer);
    writer.write(value);
    writer.finish();
    CHECK(buffer.len() == 8);
    CHECK(buffer.data()[0] == 1);
    CHECK(buffer.data()[4] == 5);
}

TEST_CASE("Archive malformed input fails without crashing")
{
    ArchiveData source = makeData();
    ByteBuffer complete;
    BinaryWriter writer(complete);
    writer.write(source);
    writer.finish();
    ByteBuffer truncated(complete.data(), complete.len() - 1);
    ArchiveData decoded;
    BinaryReader reader(truncated);
    CHECK_FALSE(reader.read(decoded));

    const uint8_t malformedVariant[] = {9};
    ByteBuffer malformed(const_cast<uint8_t*>(malformedVariant), 1);
    std::variant<int32_t, std::string> choice;
    BinaryReader malformedReader(malformed);
    CHECK_FALSE(malformedReader.read(choice));
}

TEST_CASE("Hash uses canonical archive bytes")
{
    ArchiveData first = makeData();
    ArchiveData same  = makeData();
    CHECK(hashCanonical(first) == hashCanonical(same));
    same.name = "changed";
    CHECK(hashCanonical(first) != hashCanonical(same));
    CHECK(xxh64(nullptr, 0) == 0xef46db3751d8e999ULL);
}

TEST_CASE("SchemaHash changes when a field is added")
{
    CHECK(reflect::schemaHash<SchemaBefore>() != reflect::schemaHash<SchemaAfter>());
    CHECK(reflect::schemaHash<SchemaTwinOne>() == reflect::schemaHash<SchemaTwinTwo>());
    CHECK_FALSE(reflect::isSupported<float>);
    CHECK(reflect::isSupported<ArchiveData>);
}

TEST_CASE("ByteBuffer zero capacity owns a null pointer")
{
    ByteBuffer buffer(nullptr, 0);
    CHECK(buffer.data() == nullptr);
    CHECK(buffer.len() == 0);
}

TEST_CASE("ByteBuffer resize preserves unread bytes")
{
    std::array<uint8_t, 8> bytes{1, 2, 3, 4, 5, 6, 7, 8};
    ResizableBuffer buffer(bytes.data(), static_cast<uint32_t>(bytes.size()));
    uint8_t first = 0;
    CHECK(buffer.getByte(first));
    CHECK(first == 1);
    buffer.resize(128);
    CHECK(buffer.data()[7] == 8);
}

TEST_CASE("ByteBuffer strings use an exact u32 length prefix")
{
    CHECK(ByteBuffer::isStringLengthValid(std::numeric_limits<uint32_t>::max()));
    CHECK_FALSE(ByteBuffer::isStringLengthValid(static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) + 1));
    ByteBuffer buffer;
    buffer.writeString(std::string("abc"));
    buffer.writeFinish();
    CHECK(buffer.len() == 7);
    CHECK(buffer.data()[0] == 0);
    CHECK(buffer.data()[3] == 3);
}
