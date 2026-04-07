#include "pch.h"
#include "partman.h"

#include "gdrv.h"
#include "GroupData.h"
#include "zdrv.h"
#include "maths.h"

short partman::_field_size[] =
{
	2, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0
};

uint16_t partman::ReadLe16(uint16_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return static_cast<uint16_t>((value >> 8) | (value << 8));
#else
	return value;
#endif
}

uint32_t partman::ReadLe32(uint32_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return (value >> 24) |
		((value << 8) & 0x00FF0000) |
		((value >> 8) & 0x0000FF00) |
		(value << 24);
#else
	return value;
#endif
}

void partman::NormalizeEntryBuffer(FieldTypes entryType, char* buffer, size_t fieldSize)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	if (!buffer || fieldSize == 0)
		return;

	if (entryType == FieldTypes::ShortValue || entryType == FieldTypes::ShortArray)
	{
		auto* data = reinterpret_cast<uint16_t*>(buffer);
		auto count = fieldSize / sizeof(uint16_t);
		for (size_t i = 0; i < count; i++)
			data[i] = ReadLe16(data[i]);
	}
	else if (entryType == FieldTypes::FloatArray)
	{
		auto* data = reinterpret_cast<uint32_t*>(buffer);
		auto count = fieldSize / sizeof(uint32_t);
		for (size_t i = 0; i < count; i++)
			data[i] = ReadLe32(data[i]);
	}
#else
	(void)entryType;
	(void)buffer;
	(void)fieldSize;
#endif
}

DatFile* partman::load_records(LPCSTR lpFileName, bool fullTiltMode)
{
	datFileHeader header{};
	dat8BitBmpHeader bmpHeader{};
	dat16BitBmpHeader zMapHeader{};

	auto fileHandle = fopen(lpFileName, "rb");
	if (fileHandle == nullptr)
		return nullptr;

	fread(&header, 1, sizeof header, fileHandle);
	header.FileSize = static_cast<int>(ReadLe32(static_cast<uint32_t>(header.FileSize)));
	header.NumberOfGroups = ReadLe16(header.NumberOfGroups);
	header.SizeOfBody = static_cast<int>(ReadLe32(static_cast<uint32_t>(header.SizeOfBody)));
	header.Unknown = ReadLe16(header.Unknown);

	if (strcmp("PARTOUT(4.0)RESOURCE", header.FileSignature) != 0)
	{
		fclose(fileHandle);
		return nullptr;
	}

	auto datFile = new DatFile();
	if (!datFile)
	{
		fclose(fileHandle);
		return nullptr;
	}

	datFile->AppName = header.AppName;
	datFile->Description = header.Description;

	if (header.Unknown)
	{
		auto unknownBuf = new char[header.Unknown];
		if (!unknownBuf)
		{
			fclose(fileHandle);
			delete datFile;
			return nullptr;
		}
		fread(unknownBuf, 1, header.Unknown, fileHandle);
		delete[] unknownBuf;
	}

	datFile->Groups.reserve(header.NumberOfGroups);
	bool abort = false;
	for (auto groupIndex = 0; !abort && groupIndex < header.NumberOfGroups; ++groupIndex)
	{
		auto entryCount = LRead<uint8_t>(fileHandle);
		auto groupData = new GroupData(groupIndex);
		groupData->ReserveEntries(entryCount);

		for (auto entryIndex = 0; entryIndex < entryCount; ++entryIndex)
		{
			auto entryData = new EntryData();
			auto entryType = static_cast<FieldTypes>(LRead<uint8_t>(fileHandle));
			entryData->EntryType = entryType;

			int fixedSize = _field_size[static_cast<int>(entryType)];
			size_t fieldSize = fixedSize >= 0 ? fixedSize : LRead<uint32_t>(fileHandle);
			entryData->FieldSize = static_cast<int>(fieldSize);

			if (entryType == FieldTypes::Bitmap8bit)
			{
				fread(&bmpHeader, 1, sizeof(dat8BitBmpHeader), fileHandle);
				bmpHeader.Width = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(bmpHeader.Width)));
				bmpHeader.Height = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(bmpHeader.Height)));
				bmpHeader.XPosition = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(bmpHeader.XPosition)));
				bmpHeader.YPosition = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(bmpHeader.YPosition)));
				bmpHeader.Size = static_cast<int>(ReadLe32(static_cast<uint32_t>(bmpHeader.Size)));

				assertm(bmpHeader.Size + sizeof(dat8BitBmpHeader) == fieldSize, "partman: Wrong bitmap field size");
				assertm(bmpHeader.Resolution <= 2, "partman: bitmap resolution out of bounds");

				auto bmp = new gdrv_bitmap8(bmpHeader);
				entryData->Buffer = reinterpret_cast<char*>(bmp);
				fread(bmp->IndexedBmpPtr, 1, bmpHeader.Size, fileHandle);
			}
			else if (entryType == FieldTypes::Bitmap16bit)
			{
				/*Full tilt has extra byte(@0:resolution) in zMap*/
				auto zMapResolution = 0u;
				if (fullTiltMode)
				{
					zMapResolution = LRead<uint8_t>(fileHandle);
					fieldSize--;
					assertm(zMapResolution <= 2, "partman: zMap resolution out of bounds");
				}

				fread(&zMapHeader, 1, sizeof(dat16BitBmpHeader), fileHandle);
				zMapHeader.Width = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(zMapHeader.Width)));
				zMapHeader.Height = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(zMapHeader.Height)));
				zMapHeader.Stride = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(zMapHeader.Stride)));
				zMapHeader.Unknown0 = static_cast<int>(ReadLe32(static_cast<uint32_t>(zMapHeader.Unknown0)));
				zMapHeader.Unknown1_0 = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(zMapHeader.Unknown1_0)));
				zMapHeader.Unknown1_1 = static_cast<int16_t>(ReadLe16(static_cast<uint16_t>(zMapHeader.Unknown1_1)));

				auto length = fieldSize - sizeof(dat16BitBmpHeader);

				auto zMap = new zmap_header_type(zMapHeader.Width, zMapHeader.Height, zMapHeader.Stride);
				zMap->Resolution = zMapResolution;
				if (zMapHeader.Stride * zMapHeader.Height * 2u == length)
				{
					fread(zMap->ZPtr1, 1, length, fileHandle);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
					auto words = static_cast<size_t>(length / sizeof(uint16_t));
					for (size_t i = 0; i < words; i++)
						zMap->ZPtr1[i] = ReadLe16(zMap->ZPtr1[i]);
#endif
				}
				else
				{
					// 3DPB .dat has zeroed zMap headers, in groups 497 and 498, skip them.
					fseek(fileHandle, static_cast<int>(length), SEEK_CUR);
				}
				entryData->Buffer = reinterpret_cast<char*>(zMap);
			}
			else
			{
				auto entryBuffer = new char[fieldSize];
				entryData->Buffer = entryBuffer;
				if (!entryBuffer)
				{
					abort = true;
					break;
				}
				fread(entryBuffer, 1, fieldSize, fileHandle);
				NormalizeEntryBuffer(entryType, entryBuffer, fieldSize);
			}

			groupData->AddEntry(entryData);
		}

		datFile->Groups.push_back(groupData);
	}

	fclose(fileHandle);
	if (datFile->Groups.size() == header.NumberOfGroups)
	{
		datFile->Finalize();
		return datFile;
	}
	delete datFile;
	return nullptr;
}
