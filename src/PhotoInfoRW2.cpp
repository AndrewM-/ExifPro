/*____________________________________________________________________________

   ExifPro Image Viewer

   Copyright (C) 2000-2015 Michael Kowalski
____________________________________________________________________________*/

#include "stdafx.h"
#include "PhotoInfoRW2.h"
#include "file.h"
#include "ExifBlock.h"
#include "data.h"
#include "JPEGDecoder.h"
#include "FileDataSource.h"
#include "Config.h"
#include "ExifTags.h"
#include "scan.h"
#include "PhotoFactory.h"
#include "Exception.h"

extern bool StripBlackFrame(Dib& dib, bool yuv);

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

namespace {
	RegisterPhotoType<PhotoInfoRW2> rw2(_T("rw2"), FT_RW2);
}


PhotoInfoRW2::PhotoInfoRW2()
{
	file_type_index_ = FT_RW2;
	jpeg_thm_data_offset_ = jpeg_data_offset_ = jpeg_data_size_ = 0;
}

PhotoInfoRW2::~PhotoInfoRW2()
{}


//int PhotoInfoRW2::GetTypeMarkerIndex() const
//{
//	return FT_RW2 + 1;	// in indicators.png
//}


//void PhotoInfoRW2::ParseMakerNote(FileStream& ifs)
//{
//	//char buf[10];
//	//ifs.Read(buf, 10);
//	uint32 base= 0;
//
////	if (memcmp(buf, "Nikon", 6) == 0)
////	{
////		base = ifs.RPosition();
////
////		uint16 order= ifs.GetUInt16();
////		if (order != 'MM' && order != 'II')
////			return;
////
//////		ifs.SetByteOrder(start[10] == 'M');
////
////		if (ifs.GetUInt16() != 0x2a)	// has to be 0x2a (this is IFD header)
////			return;
////
////		ifs.RPosition(ifs.GetUInt32() - 8);
////	}
////	else
////		ifs.RPosition(-10);
//
//	uint16 entries= ifs.GetUInt16();
//
//	if (entries > 0x200)
//	{
//		ASSERT(false);
//		return;
//	}
//
//	for (int i= 0; i < entries; ++i)
//	{
//		uint16 tag= ifs.GetUInt16();
//		Data data(ifs, base);
//		Offset temp= ifs.RPosition();
//
//TRACE(_T("mkn: %x (= %d)\n"), int(tag), int(data.GetData()));
//
//		switch (tag)
//		{
//		//case 0x11:	// thumbnail's ifd
//		//	{
//		//		ifs.RPosition(base + data.AsULong(), FileStream::beg);
//		//		uint16 ifd_entries= ifs.GetUInt16();
//
//		//		if (ifd_entries > 0x200)
//		//		{
//		//			ASSERT(false);
//		//			break;
//		//		}
//
//		//		for (int i= 0; i < ifd_entries; ++i)
//		//		{
//		//			uint16 tag= ifs.GetUInt16();
//		//			Data data(ifs, 0);
//
//		//			if (tag == 0x201)	// JPEGInterchangeFormat
//		//			{
//		//				jpeg_thm_data_offset_ = base + data.AsULong();
//		//				break;
//		//			}
//		//		}
//		//	}
//		//	break;
//
//		default:
//			break;
//		}
//
//		ifs.RPosition(temp, FileStream::beg);
//	}
//}


//void PhotoInfoRW2::ParseExif(FileStream& ifs, uint32 base)
//{
//	uint16 entries= ifs.GetUInt16();
//
//	if (entries > 0x200)
//	{
//		ASSERT(false);
//		return;
//	}
//
//	for (int i= 0; i < entries; ++i)
//	{
//		uint16 tag= ifs.GetUInt16();
//		Data data(ifs, 0);
//		Offset temp= ifs.RPosition();
//
////TRACE(_T("exf: %x (= %d)\n"), int(tag), int(data.GetData()));
//
//		switch (tag)
//		{
//		case 0x927c:
//			ifs.RPosition(data.GetData(), FileStream::beg);
//			ParseMakerNote(ifs);
//			break;
//
//		default:
//			break;
//		}
//
//		ifs.RPosition(temp, FileStream::beg);
//	}
//}


uint32 PhotoInfoRW2::ParseIFD(FileStream& ifs, uint32 base, bool main_image)
{
	uint16 entries= ifs.GetUInt16();

	if (entries > 0x200)
	{
		ASSERT(false);
		return 0;
	}

	uint32 exif_offset= 0;

	for (int i= 0; i < entries; ++i)
	{
		Offset tag_start= ifs.RPosition();

		uint16 tag= ifs.GetUInt16();
		Data data(ifs, 0);

		Offset temp= ifs.RPosition();

		// TODO: read time and orientation
		//...

TRACE(_T("rw2: %x (= %d)\n"), int(tag), int(data.GetData()));

//		if (main_image)
//			output_.RecordInfo(tag, TagName(tag), data);

		switch (tag)
		{
//		case 0xfe:	// sub file?
//			main_image = (data.AsULong() & 1) == 0;	// main image?
//			break;

		case 0x2:		// img width
//			if (main_image)
				SetWidth(data.AsULong());
			break;

		case 0x3:		// img height
//			if (main_image)
				SetHeight(data.AsULong());
			break;

		case TIFF_ORIENTATION:		// orientation
//			if (main_image)
				SetOrientation(static_cast<uint16>(data.AsULong()));
			break;

		case 0x2e:		// embedded JPEG
			jpeg_data_offset_ = data.GetData();
			jpeg_data_size_ = data.Components();
			break;

		case 0x8769:	// EXIF tag
			exif_offset = data.AsULong();
//			ifs.RPosition(exif_offset, FileStream::beg);
//			ParseExif(ifs, exif_offset);
			break;

		case 0x8825:	// GPSInfo
			SetGpsData(ReadGPSInfo(ifs, 0, data, &output_));
			break;
		}
/*
		if (main_image)
		{
			ifs.RPosition(tag_start, FileStream::beg);
//			ReadEntry(ifs, base, make_, model_, this, output_);
			//output_.RecordInfo(tag, TagName(tag), data);
		}
*/
		ifs.RPosition(temp, FileStream::beg);
	}

	return exif_offset;
}


bool PhotoInfoRW2::Scan(const TCHAR* file_name, ExifBlock& exifData, bool generateThumbnails, ImgLogger* logger)
{
	exifData.clear();

	bool exif_present= false;

	// scan Panasonic RW2

	FileStream ifs;

	if (!ifs.Open(file_name))
		return exif_present;

	if (ifs.GetLength32() < 200)	// too short?
		return exif_present;

	uint16 order= ifs.GetUInt16();
	if (order == 'MM')
		ifs.SetByteOrder(true);
	else if (order == 'II')
		ifs.SetByteOrder(false);
	else
		return exif_present;

	if (ifs.GetUInt16() != 0x55)	// RW2 magic number
		return exif_present;

	uint32 exif_offset= 0;

	for (int i= 0; i < 9; ++i)		// limit no of IFDs
	{
		uint32 offset= ifs.GetUInt32();		// IFD offset
		if (offset == 0)
			break;

		ifs.RPosition(offset, FileStream::beg);	// go to the IFD

		if (uint32 offset= ParseIFD(ifs, 0, i == 0))
			exif_offset = offset;
	}

	if (jpeg_data_offset_ == 0 && jpeg_thm_data_offset_ == 0)
		THROW_EXCEPTION(L"Unsupported RW2 file format", L"This file is not supported. Cannot find embedded preview image.");

	if (exif_offset)
	{
		if (jpeg_data_offset_)
		{
			ifs.RPosition(jpeg_data_offset_, FileStream::beg);	// go to JPG preview
			exif_present = ::Scan(file_name, ifs, this, &output_, 0, true, 0);
		}

		ifs.RPosition(exif_offset, FileStream::beg);	// go to EXIF

		// IFD offset seems to be 0
		uint32 ifd_offset= 0;
		uint32 exif_size= ifs.GetLength32() - exif_offset;	// approximation only
		std::pair<uint32, uint32> range(exif_offset, exif_size);
		exif_present = ScanExif(file_name, ifs, ifd_offset, range, _T("Panasonic"), String(), this, &exifData, &output_, false);

		SetExifInfo(exif_offset, exif_size, ifd_offset, ifs.GetByteOrder());
	}

	return exif_present;
}


bool PhotoInfoRW2::IsRotationFeasible() const
{
	return false;
}


bool PhotoInfoRW2::ReadExifBlock(ExifBlock& exif) const
{
	return false;


//	if (!exif_data_present_)
		return false;
}


ImageStat PhotoInfoRW2::CreateThumbnail(Dib& bmp, DecoderProgress* progress) const
{
	try
	{
		uint32 offset= jpeg_thm_data_offset_ ? jpeg_thm_data_offset_ : jpeg_data_offset_;

		CFileDataSource fsrc(path_.c_str(), offset);
		JPEGDecoder dec(fsrc, JDEC_INTEGER_HIQ);
		dec.SetFast(true, true);
		dec.SetProgressClient(progress);
		ImageStat stat= dec.DecodeImg(bmp, GetThumbnailSize(), true);

		if (stat == IS_OK && bmp.IsValid() && jpeg_thm_data_offset_)
			StripBlackFrame(bmp, false);

		return stat;
	}
	catch (...)	// for now (TODO--failure codes)
	{
	}
	return IS_READ_ERROR;
}


CImageDecoderPtr PhotoInfoRW2::GetDecoder() const
{
	AutoPtr<JPEGDataSource> file= new CFileDataSource(path_.c_str(), jpeg_data_offset_);
	JpegDecoderMethod dct_method= g_Settings.dct_method_; // lousy
	return new JPEGDecoder(file, dct_method);
}


void PhotoInfoRW2::CompleteInfo(ImageDatabase& db, OutputStr& out) const
{
	out = output_;
}


bool PhotoInfoRW2::GetEmbeddedJpegImage(uint32& jpeg_data_offset, uint32& jpeg_data_size) const
{
	if (jpeg_data_offset_ && jpeg_data_size_)
	{
		jpeg_data_offset = jpeg_data_offset_;
		jpeg_data_size = jpeg_data_size_;
		return true;
	}
	else
		return false;
}
