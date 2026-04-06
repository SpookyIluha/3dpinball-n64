#pragma once

struct zmap_header_type;
struct gdrv_bitmap8;
enum class FieldTypes : int16_t;


enum class bmp8Flags : unsigned char
{
	RawBmpUnaligned = 1 << 0,
	DibBitmap = 1 << 1,
	Spliced = 1 << 2,
};


#pragma pack(push, 1)
struct datFileHeader
{
	char FileSignature[21];
	char AppName[50];
	char Description[100];
	int FileSize;
	unsigned short NumberOfGroups;
	int SizeOfBody;
	unsigned short Unknown;
};

struct dat8BitBmpHeader
{
	uint8_t Resolution;
	int16_t Width;
	int16_t Height;
	int16_t XPosition;
	int16_t YPosition;
	int Size;
	bmp8Flags Flags;

	bool IsFlagSet(bmp8Flags flag) const
	{
		return static_cast<char>(Flags) & static_cast<char>(flag);
	}
};

struct dat16BitBmpHeader
{
	int16_t Width;
	int16_t Height;
	int16_t Stride;
	int Unknown0;
	int16_t Unknown1_0;
	int16_t Unknown1_1;
};
#pragma pack(pop)

static_assert(sizeof(dat8BitBmpHeader) == 14, "Wrong size of dat8BitBmpHeader");
static_assert(sizeof(datFileHeader) == 183, "Wrong size of datFileHeader");
static_assert(sizeof(dat16BitBmpHeader) == 14, "Wrong size of zmap_header_type");


class partman
{
public:
	static class DatFile* load_records(LPCSTR lpFileName, bool fullTiltMode);
private:
	static short _field_size[];
	static void NormalizeEntryBuffer(FieldTypes entryType, char* buffer, size_t fieldSize);
	static uint16_t ReadLe16(uint16_t value);
	static uint32_t ReadLe32(uint32_t value);

	template <typename T>
	static T LRead(FILE* file)
	{
		T Buffer{};
		fread(&Buffer, 1, sizeof(T), file);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		if constexpr (sizeof(T) == 2)
		{
			uint16_t raw;
			std::memcpy(&raw, &Buffer, sizeof(raw));
			raw = ReadLe16(raw);
			std::memcpy(&Buffer, &raw, sizeof(raw));
		}
		else if constexpr (sizeof(T) == 4)
		{
			uint32_t raw;
			std::memcpy(&raw, &Buffer, sizeof(raw));
			raw = ReadLe32(raw);
			std::memcpy(&Buffer, &raw, sizeof(raw));
		}
#endif
		return Buffer;
	}
};
