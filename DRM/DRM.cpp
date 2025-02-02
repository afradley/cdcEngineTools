#include "DRM.h"
#include "File.h"
#include "FileExtensions.h"

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include <assert.h>

cDRM::cDRM()
{
	this->m_version = 0;
	this->m_numSections = 0;
	if (this->m_sections.size() > 0)
	{
		this->m_sections.clear();
	}
}

cDRM::~cDRM()
{
	this->m_version = 0;
	this->m_numSections = 0;
	if (this->m_sections.size() > 0)
	{
		this->m_sections.clear();
	}
}

void cDRM::ExtractSections(char* szFilePath)
{
	//Store DRM input path
	this->m_filePath = szFilePath;

	//Initialise ifstream for reading in binary mode
	std::ifstream ifs(this->m_filePath, std::ios::binary);

	//If it's not good to go
	if (!ifs.good())
	{
		std::cout << "Fatal Error: Failed to open file at path: " << this->m_filePath << std::endl;
		return;
	}

	//Read our DRM header into cDRM
	this->m_version = ReadUInt(ifs);
	if (this->m_version != DRM_VERSION)
	{
		std::cout << "Fatal Error: Version mis-match! expected: " << DRM_VERSION << " got: " << this->m_version << std::endl;
		return;
	}

#if TR8
	this->m_nameSize = ReadUInt(ifs);
	this->m_paddingSize = ReadUInt(ifs);
	this->m_unk00 = ReadUInt(ifs);
	this->m_unk01 = ReadUInt(ifs);
#elif TRAS
	this->m_nameSize = ReadUInt(ifs);
	this->m_paddingSize = ReadUInt(ifs);
	this->m_unk00 = ReadUInt(ifs);
	this->m_unk01 = ReadUInt(ifs);
	this->m_unk02 = ReadUInt(ifs);
#endif

	this->m_numSections = ReadUInt(ifs);
	if (this->m_numSections > DRM_MAX_SECTIONS)
	{
		std::cout << "Fatal Error: Number of Sections: " << this->m_numSections << " exceeded the limit of: " << DRM_MAX_SECTIONS << "!" << std::endl;
		return;
	}

	//If not enough sections
	if (this->m_numSections <= 0)
	{
		std::cout << "Fatal Error: Number of Sections <= 0 !" << std::endl;
		return;
	}

#if TRAS
	ifs.seekg(0x4, SEEK_CUR);
#endif

	//Read all the section info into this
	for (int i = 0; i != this->m_numSections; i++)
	{
		this->m_sections.emplace_back();
		Section* section = &this->m_sections[this->m_sections.size() - 1];

		section->uiSize = ReadUInt(ifs);
		section->ucType = ReadUByte(ifs);
		section->ucUnk00 = ReadUByte(ifs);
		section->usUnk01 = ReadUShort(ifs);
#ifdef LOK5
		section->ucType = 0; // No section type enum available in LOK5, but 2 seems to be animations.
		section->uiHash = ReadUInt(ifs);
#else
		section->uiHeaderSize = ReadUInt(ifs);	
		section->uiHash = ReadUInt(ifs);
		section->uiLang = ReadUInt(ifs);
#endif
	}

#if TR8 || TRAS
	//Skip past names & padding info
	ifs.seekg(this->m_nameSize + this->m_paddingSize, SEEK_CUR);
#endif

	std::string strOutPath = std::string(this->m_filePath);
	strOutPath = strOutPath.substr(0, strOutPath.find_last_of(".")) + "\\";;

	//Create Directories
	CreateDirectories(strOutPath);

	for (int i = 0; i != this->m_numSections; i++)
	{
		Section* section = &this->m_sections[i];

		//Print extraction message to console
		std::cout << "Extracting Section: " << "[ " << (i + 1) << " / " << this->m_numSections << " ]" << std::endl;

		//Define output file path
		std::stringstream strOutPath2;
		strOutPath2 << strOutPath << i << "_" << std::hex << section->uiHash << szExtensions[section->ucType];

		std::stringstream strOutPath3;
		strOutPath3 << strOutPath << "\\sectionList.txt";

		//Skip header
#if REPACK_MODE
		bool bRealSize = false;//Don't modify
#else
		bool bRealSize = true;
#endif
		
		if (bRealSize)
		{
			//Declare variables to store data
			char* szSectionData = new char[section->uiSize];

			//Declare output file stream
			std::ofstream ofs(strOutPath2.str(), std::ios::binary);

			//Create output sectionList.txt
			std::ofstream ofs2(strOutPath3.str(), std::ios::app);

			ofs2 << i << "_" << std::hex << section->uiHash << szExtensions[section->ucType] << std::endl;
			
			//If not good to go
			if (!ofs.good())
			{
				std::cout << "Fatal Error: Unknown error occured whilst initialising ofstream!" << std::endl;
				return;
			}

			//Skip header info
#if LOK5
			section->uiHeaderSize = ReadUInt(ifs);
			ifs.seekg((section->uiHeaderSize << 0x2), SEEK_CUR);
#elif TR7 || TRAE
			ifs.seekg(((section->uiHeaderSize >> 0x8) * 0x8), SEEK_CUR);
#elif TR8
			ifs.seekg((section->uiHeaderSize >> 0x8), SEEK_CUR);
#elif TRAS
			ifs.seekg((section->uiHeaderSize >> 0x8), SEEK_CUR);
#else
#error "Unsupported Game!"
#endif
			//Read then write the section data
			ifs.read(szSectionData, section->uiSize);

#if REPACK_MODE
	#if LOK5
			//Write section header
			WriteUInt(ofs, section->uiSize);
			WriteUByte(ofs, section->ucType);
			WriteUByte(ofs, section->ucUnk00);
			WriteUShort(ofs, section->usUnk01);
			WriteUInt(ofs, section->uiHash);
	#elif (TR7 || TRAE)
			//Write section header
			WriteUInt(ofs, 0x54434553);
			WriteUInt(ofs, section->uiSize);
			WriteUByte(ofs, section->ucType);
			WriteUByte(ofs, section->ucUnk00);
			WriteUShort(ofs, section->usUnk01);
			WriteUInt(ofs, section->uiHeaderSize);
			WriteUInt(ofs, section->uiHash);
			WriteUInt(ofs, section->uiLang);
	#endif
#endif
			ofs.write(szSectionData, section->uiSize);
			
			//Flush and close ofstream
			ofs.flush();
			ofs.close();

			//
			ofs2.flush();
			ofs2.close();

			//delete allocated section data
			delete[] szSectionData;
		}
		else
		{
			//Declare char* to store section data
#if LOK5
			section->uiHeaderSize = ReadUInt(ifs);
			char* szSectionData = new char[section->uiSize + (section->uiHeaderSize << 2)];
#elif TR7 || TRAE
			char* szSectionData = new char[section->uiSize + ((section->uiHeaderSize >> 0x8) * 0x8)];
#elif TR8
			char* szSectionData = new char[section->uiSize + (section->uiHeaderSize >> 0x8)];
#elif TRAS
			char* szSectionData = new char[section->uiSize + (section->uiHeaderSize >> 0x8)];
#endif

			//Declare output file stream
			std::ofstream ofs(strOutPath2.str(), std::ios::binary);

			//Create output sectionList.txt
			std::ofstream ofs2(strOutPath3.str(), std::ios::app);

			ofs2 << i << "_" << std::hex << section->uiHash << szExtensions[section->ucType] << std::endl;

			//If not good to go
			if (!ofs.good())
			{
				std::cout << "Fatal Error: Unknown error occured whilst initialising ofstream!" << std::endl;
				return;
			}

			//Read then write the section data
#if REPACK_MODE
	#if LOK5
			//Write section header
			WriteUInt(ofs, section->uiSize);
			WriteUByte(ofs, section->ucType);
			WriteUByte(ofs, section->ucUnk00);
			WriteUShort(ofs, section->usUnk01);
			WriteUInt(ofs, section->uiHash);
	#else
			//Write section header
			WriteUInt(ofs, 0x54434553);
			WriteUInt(ofs, section->uiSize);
			WriteUByte(ofs, section->ucType);
			WriteUByte(ofs, section->ucUnk00);
			WriteUShort(ofs, section->usUnk01);
			WriteUInt(ofs, section->uiHeaderSize);
			WriteUInt(ofs, section->uiHash);
			WriteUInt(ofs, section->uiLang);
	#endif
#endif

#if LOK5
			ifs.read(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x2));
			ofs.write(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x2));
#elif TR7 || TRAE
			ifs.read(szSectionData, section->uiSize + ((section->uiHeaderSize >> 0x8) * 0x8));
			ofs.write(szSectionData, section->uiSize + ((section->uiHeaderSize >> 0x8) * 0x8));
#elif TR8
			ifs.read(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x8));
			ofs.write(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x8));
#elif TRAS
			ifs.read(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x8));
			ofs.write(szSectionData, section->uiSize + (section->uiHeaderSize >> 0x8));
#endif
			//Flush and close ofstream
			ofs.flush();
			ofs.close();

			//Flush and close ofstream
			ofs2.flush();
			ofs2.close();

			//Delete allocated section data
			delete[] szSectionData;
		}
	}

	//Close ifstream
	ifs.close();

	//Print success
	std::cout << "Successfully Extracted: " << "[ " << (this->m_numSections) << " ] " << " section(s)!" << std::endl;
}