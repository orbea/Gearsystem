/*
 * Gearsystem - Sega Master System / Game Gear Emulator
 * Copyright (C) 2013  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include <string>
#include <algorithm>
#include <ctype.h>
#include "Cartridge.h"
#include "miniz/miniz.c"
#include "game_db.h"

Cartridge::Cartridge()
{
    InitPointer(m_pROM);
    m_iROMSize = 0;
    m_Type = CartridgeNotSupported;
    m_Zone = CartridgeUnknownZone;
    m_bValidROM = false;
    m_bReady = false;
    m_szFilePath[0] = 0;
    m_szFileName[0] = 0;
    m_iROMBankCount = 0;
    m_bGameGear = false;
    m_bPAL = false;
    m_bRAMWithoutBattery = false;
}

Cartridge::~Cartridge()
{
    SafeDeleteArray(m_pROM);
}

void Cartridge::Init()
{
    Reset();
}

void Cartridge::Reset()
{
    SafeDeleteArray(m_pROM);
    m_iROMSize = 0;
    m_Type = CartridgeNotSupported;
    m_Zone = CartridgeUnknownZone;
    m_bValidROM = false;
    m_bReady = false;
    m_szFilePath[0] = 0;
    m_szFileName[0] = 0;
    m_iROMBankCount = 0;
    m_bGameGear = false;
    m_bPAL = false;
    m_bRAMWithoutBattery = false;
    m_GameGenieList.clear();
}

bool Cartridge::IsGameGear() const
{
    return m_bGameGear;
}

bool Cartridge::IsPAL() const
{
    return m_bPAL;
}

bool Cartridge::IsValidROM() const
{
    return m_bValidROM;
}

bool Cartridge::IsReady() const
{
    return m_bReady;
}
bool Cartridge::HasRAMWithoutBattery() const
{
    return m_bRAMWithoutBattery;
}

Cartridge::CartridgeTypes Cartridge::GetType() const
{
    return m_Type;
}

Cartridge::CartridgeZones Cartridge::GetZone() const
{
    return m_Zone;
}

void Cartridge::ForzeZone(Cartridge::CartridgeZones zone)
{
    m_Zone = zone;
}

int Cartridge::GetROMSize() const
{
    return m_iROMSize;
}

int Cartridge::GetROMBankCount() const
{
    return m_iROMBankCount;
}

const char* Cartridge::GetFilePath() const
{
    return m_szFilePath;
}

const char* Cartridge::GetFileName() const
{
    return m_szFileName;
}

u8* Cartridge::GetROM() const
{
    return m_pROM;
}

bool Cartridge::LoadFromZipFile(const u8* buffer, int size)
{
    using namespace std;

    mz_zip_archive zip_archive;
    mz_bool status;
    memset(&zip_archive, 0, sizeof (zip_archive));

    status = mz_zip_reader_init_mem(&zip_archive, (void*) buffer, size, 0);
    if (!status)
    {
        Log("mz_zip_reader_init_mem() failed!");
        return false;
    }

    for (unsigned int i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
            Log("mz_zip_reader_file_stat() failed!");
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        Log("ZIP Content - Filename: \"%s\", Comment: \"%s\", Uncompressed size: %u, Compressed size: %u", file_stat.m_filename, file_stat.m_comment, (unsigned int) file_stat.m_uncomp_size, (unsigned int) file_stat.m_comp_size);

        string fn((const char*) file_stat.m_filename);
        transform(fn.begin(), fn.end(), fn.begin(), (int(*)(int)) tolower);
        string extension = fn.substr(fn.find_last_of(".") + 1);

        if ((extension == "sms") || (extension == "gg"))
        {
            m_bGameGear = (extension == "gg");

            void *p;
            size_t uncomp_size;

            p = mz_zip_reader_extract_file_to_heap(&zip_archive, file_stat.m_filename, &uncomp_size, 0);
            if (!p)
            {
                Log("mz_zip_reader_extract_file_to_heap() failed!");
                mz_zip_reader_end(&zip_archive);
                return false;
            }

            bool ok = LoadFromBuffer((const u8*) p, (int)uncomp_size);

            free(p);
            mz_zip_reader_end(&zip_archive);

            return ok;
        }
    }
    return false;
}

bool Cartridge::LoadFromFile(const char* path)
{
    using namespace std;

    Log("Loading %s...", path);

    Reset();

    strcpy(m_szFilePath, path);

    std::string pathstr(path);
    std::string filename;

    size_t pos = pathstr.find_last_of("\\");
    if (pos != std::string::npos)
    {
        filename.assign(pathstr.begin() + pos + 1, pathstr.end());
    }
    else
    {
        pos = pathstr.find_last_of("/");
        if (pos != std::string::npos)
        {
            filename.assign(pathstr.begin() + pos + 1, pathstr.end());
        }
        else
        {
            filename = pathstr;
        }
    }

    strcpy(m_szFileName, filename.c_str());

    ifstream file(path, ios::in | ios::binary | ios::ate);

    if (file.is_open())
    {
        int size = static_cast<int> (file.tellg());
        char* memblock = new char[size];
        file.seekg(0, ios::beg);
        file.read(memblock, size);
        file.close();

        string fn(path);
        transform(fn.begin(), fn.end(), fn.begin(), (int(*)(int)) tolower);
        string extension = fn.substr(fn.find_last_of(".") + 1);

        if (extension == "zip")
        {
            Log("Loading from ZIP...");
            m_bReady = LoadFromZipFile(reinterpret_cast<u8*> (memblock), size);
        }
        else
        {
            m_bGameGear = (extension == "gg");
            m_bReady = LoadFromBuffer(reinterpret_cast<u8*> (memblock), size);
        }

        if (m_bReady)
        {
            Log("ROM loaded", path);
        }
        else
        {
            Log("There was a problem loading the memory for file %s...", path);
        }

        SafeDeleteArray(memblock);
    }
    else
    {
        Log("There was a problem loading the file %s...", path);
        m_bReady = false;
    }

    if (!m_bReady)
    {
        Reset();
    }

    return m_bReady;
}

bool Cartridge::LoadFromBuffer(const u8* buffer, int size)
{
    if (IsValidPointer(buffer))
    {
        Log("Loading from buffer... Size: %d", size);
        // Some ROMs have 512 Byte File Headers
        if ((size % 1024) == 512)
        {
            buffer += 512;
            size -= 512;
            Log("Invalid size found. ROM trimmed to %d bytes", size);
        }
        // Unkown size
        else if ((size % 1024) != 0)
        {
            Log("Invalid size found. %d bytes", size);
            return false;
        }

        m_iROMSize = size;
        m_pROM = new u8[m_iROMSize];
        memcpy(m_pROM, buffer, m_iROMSize);

        m_bReady = true;

        u32 crc = CalculateCRC32(0, m_pROM, m_iROMSize);

        return GatherMetadata(crc);
    }
    else
        return false;
}

unsigned int Cartridge::Pow2Ceil(u16 n)
{
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    ++n;
    return n;
}

bool Cartridge::TestValidROM(u16 location)
{
    if (location + 0x10 > m_iROMSize)
        return false;

    char tmrsega[9] = {0};
    tmrsega[8] = 0;

    for (int i = 0; i < 8; i++)
    {
        tmrsega[i] = m_pROM[location + i];
    }

    return (strcmp(tmrsega, "TMR SEGA") == 0);
}

bool Cartridge::GatherMetadata(u32 crc)
{
    u16 headerLocation = 0x7FF0;
    m_bValidROM = true;

    if (!TestValidROM(headerLocation))
    {
        headerLocation = 0x1FF0;
        if (!TestValidROM(headerLocation))
        {
            headerLocation = 0x3FF0;
            if (!TestValidROM(headerLocation))
            {
                m_bValidROM = false;
            }
        }
    }

    if (m_bValidROM)
    {
        Log("ROM is Valid. Header found at: %X", headerLocation);
    }
    else
    {
        Log("ROM is NOT Valid. No header found");
    }

    u8 zone = (m_pROM[headerLocation + 0x0F] >> 4) & 0x0F;

    switch (zone)
    {
        case 3:
        {
            m_Zone = CartridgeJapanSMS;
            Log("Cartridge zone is SMS Japan");
            break;
        }
        case 4:
        {
            m_Zone = CartridgeExportSMS;
            Log("Cartridge zone is SMS Export");
            break;
        }
        case 5:
        {
            m_Zone = CartridgeJapanGG;
            m_bGameGear = true;
            Log("Cartridge zone is GG Japan");
            break;
        }
        case 6:
        {
            m_Zone = CartridgeExportGG;
            m_bGameGear = true;
            Log("Cartridge zone is GG Export");
            break;
        }
        case 7:
        {
            m_Zone = CartridgeInternationalGG;
            m_bGameGear = true;
            Log("Cartridge zone is GG International");
            break;
        }
        default:
        {
            m_Zone = CartridgeUnknownZone;
            Log("Unknown cartridge zone");
            break;
        }
    }

    m_iROMBankCount = std::max(Pow2Ceil(m_iROMSize / 0x4000), 1u);

    Log("ROM Size: %d KB", m_iROMSize / 1024);
    Log("ROM Bank Count: %d", m_iROMBankCount);

    if (m_iROMSize <= 0xC000)
    {
        // size <= 48KB
        m_Type = Cartridge::CartridgeRomOnlyMapper;
    }
    else
    {
        m_Type = Cartridge::CartridgeSegaMapper;
    }

    GetInfoFromDB(crc);

    switch (m_Type)
    {
        case Cartridge::CartridgeRomOnlyMapper:
            Log("NO mapper found");
            break;
        case Cartridge::CartridgeSegaMapper:
            Log("SEGA mapper found");
            break;
        case Cartridge::CartridgeCodemastersMapper:
            Log("Codemasters mapper found");
            break;
        case Cartridge::CartridgeNotSupported:
            Log("Cartridge not supported!!");
            break;
        default:
            Log("ERROR with cartridge type!!");
            break;
    }

    if (m_bGameGear)
    {
        Log("Game Gear cartridge identified");
    }

    return (m_Type != CartridgeNotSupported);
}

void Cartridge::GetInfoFromDB(u32 crc)
{
    int i = 0;
    bool found = false;

    while(!found && (kGameDatabase[i].title != 0))
    {
        u32 db_crc = kGameDatabase[i].crc;

        if (db_crc == crc)
        {
            found = true;

            Log("ROM found in database: %s. CRC: %X", kGameDatabase[i].title, crc);

            if (kGameDatabase[i].mapper == GS_DB_CODEMASTERS_MAPPER)
                m_Type = Cartridge::CartridgeCodemastersMapper;

            if (kGameDatabase[i].sms_mode)
            {
                Log("Forcing Master System mode");
                m_bGameGear = false;
            }

            if (kGameDatabase[i].pal)
            {
                Log("PAL cartridge: Running at 50Hz");
                m_bPAL = true;
            }

            if (kGameDatabase[i].no_battery)
            {
                Log("Cartridge with SRAM but no battery");
                m_bRAMWithoutBattery = true;
            }
        }
        else
            i++;
    }

    if (!found)
    {
        Log("ROM not found in database. CRC: %X", crc);
    }
}

void Cartridge::SetGameGenieCheat(const char* szCheat)
{
    std::string code(szCheat);
    for (std::string::iterator p = code.begin(); code.end() != p; ++p)
        *p = toupper(*p);

    if (m_bReady && (code.length() > 6) && ((code[3] < '0') || ((code[3] > '9') && (code[3] < 'A'))))
    {
        u8 new_value = (AsHex(code[0]) << 4 | AsHex(code[1])) & 0xFF;
        u16 cheat_address = (AsHex(code[2]) << 8 | AsHex(code[4]) << 4 | AsHex(code[5]) | (AsHex(code[6]) ^ 0xF) << 12) & 0xFFFF;
        bool avoid_compare = true;
        u8 compare_value = 0;

        if ((code.length() == 11) && ((code[7] < '0') || ((code[7] > '9') && (code[7] < 'A'))))
        {
            compare_value = (AsHex(code[8]) << 4 | AsHex(code[10])) ^ 0xFF;
            compare_value = ((compare_value >> 2 | compare_value << 6) ^ 0x45) & 0xFF;
            avoid_compare = false;
        }

        for (int bank = 0; bank < GetROMBankCount(); bank++)
        {
            int bank_address = (bank * 0x4000) + (cheat_address & 0x3FFF);

            if (avoid_compare || (m_pROM[bank_address] == compare_value))
            {
                GameGenieCode undo_data;
                undo_data.address = bank_address;
                undo_data.old_value = m_pROM[bank_address];

                m_pROM[bank_address] = new_value;

                m_GameGenieList.push_back(undo_data);
            }
        }
    }
}

void Cartridge::ClearGameGenieCheats()
{
    std::list<GameGenieCode>::iterator it;

    for (it = m_GameGenieList.begin(); it != m_GameGenieList.end(); it++)
    {
        m_pROM[it->address] = it->old_value;
    }

    m_GameGenieList.clear();
}
