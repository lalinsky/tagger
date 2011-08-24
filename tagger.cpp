/* Copyright (C) 2011 Lukas Lalinsky <lalinsky@gmail.com>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include <tbytevector.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v2frame.h>
#include <id3v2header.h>
#include <urllinkframe.h>
#include <uniquefileidentifierframe.h>
#include <textidentificationframe.h>
#include <attachedpictureframe.h>
#include <tfilestream.h>

using namespace std;
using namespace TagLib;

struct Metadata {
	Metadata() :
		hasArtist(0),
		hasAlbum(0),
		hasAlbumArtist(0),
		hasTitle(0),
		hasGenre(0),
		hasTrackNumber(0),
		hasTrackCount(0),
		hasYear(0),
		hasImage(0),
		hasPublisher(0)
	{}
	String artist, album, albumartist, title, genre, publisher;
	Map<String, String> customUserTextFrames;
	Map<String, String> customTextFrames;
	Map<String, String> customUserUrlFrames;
	Map<String, String> customUrlFrames;
	Map<String, String> customUniqueFileIdentifierFrames;
	int trackNumber, trackCount, year;
	ByteVector image;
	int hasArtist : 1;
	int hasAlbum : 1;
	int hasAlbumArtist : 1;
	int hasTitle : 1;
	int hasGenre : 1;
	int hasTrackNumber : 1;
	int hasTrackCount : 1;
	int hasYear : 1;
	int hasImage : 1;
	int hasPublisher : 1;
};

void usage()
{
	cout << endl;
	cout << "Usage: zvq-tagger <fields> <files>" << endl;
	cout << endl;
	cout << "Where the valid fields are:" << endl;
	cout << "  -t <title>"   << endl;
	cout << "  -a <artist>"  << endl;
	cout << "  -A <album>"   << endl;
	cout << "  -b <albumartist>"  << endl;
	cout << "  -n <track number>" << endl;
	cout << "  -N <track count>" << endl;
	cout << "  -G <genre>"   << endl;
	cout << "  -Y <year>"    << endl;
	cout << "  -p <publisher>"   << endl;
	cout << "  -i <image>"   << endl;
	cout << endl;

	exit(1);
}

ByteVector readFile(char *path)
{
	FileStream imageStream(optarg);
	return imageStream.readBlock(imageStream.length());
}

void setTextFrame(ID3v2::Tag *tag, const char *id, const String &text)
{
    ID3v2::TextIdentificationFrame *f = new ID3v2::TextIdentificationFrame(id, String::UTF16);
    f->setText(text);
	tag->removeFrames(id);
    tag->addFrame(f);
}

void setUserTextFrame(ID3v2::Tag *tag, const String &description, const String &text)
{
    ID3v2::UserTextIdentificationFrame *f = new ID3v2::UserTextIdentificationFrame(String::UTF16);
    f->setDescription(description);
    f->setText(text);
    ID3v2::UserTextIdentificationFrame *of = ID3v2::UserTextIdentificationFrame::find(tag, description);
	if (of) {
		tag->removeFrame(of);
	}
    tag->addFrame(f);
}

void setUrlFrame(ID3v2::Tag *tag, const char *id, const String &text)
{
    ID3v2::UrlLinkFrame *f = new ID3v2::UrlLinkFrame(id);
    f->setUrl(text);
	tag->removeFrames(id);
    tag->addFrame(f);
}

ID3v2::UserUrlLinkFrame *findUserUrlLinkFrame(ID3v2::Tag *tag, const String &description)
{
	ID3v2::FrameList l = tag->frameList("WXXX");
	for (ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
		ID3v2::UserUrlLinkFrame *f = dynamic_cast<ID3v2::UserUrlLinkFrame *>(*it);
		if (f && f->description() == description) {
			return f;
		}
	}
	return 0;
}

void setUserUrlFrame(ID3v2::Tag *tag, const String &description, const String &text)
{
    ID3v2::UserUrlLinkFrame *f = new ID3v2::UserUrlLinkFrame(String::UTF16);
    f->setDescription(description);
    f->setUrl(text);
    ID3v2::UserUrlLinkFrame *of = findUserUrlLinkFrame(tag, description);
	if (of) {
		tag->removeFrame(of);
	}
    tag->addFrame(f);
}

ID3v2::UniqueFileIdentifierFrame *findUniqueFileIdentifierFrame(ID3v2::Tag *tag, const String &owner)
{
	ID3v2::FrameList l = tag->frameList("UFID");
	for (ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
		ID3v2::UniqueFileIdentifierFrame *f = dynamic_cast<ID3v2::UniqueFileIdentifierFrame *>(*it);
		if (f && f->owner() == owner) {
			return f;
		}
	}
	return 0;
}

void setUniqueFileIdentifierFrame(ID3v2::Tag *tag, const String &owner, const String &identifier)
{
    ID3v2::UniqueFileIdentifierFrame *f = new ID3v2::UniqueFileIdentifierFrame(owner, identifier.toCString(true));
    ID3v2::UniqueFileIdentifierFrame *of = findUniqueFileIdentifierFrame(tag, owner);
	if (of) {
		tag->removeFrame(of);
	}
    tag->addFrame(f);
}

String detectImageMimeType(const ByteVector &image)
{
	if (image.size() > 4) {
		const unsigned char *data = (const unsigned char *)image.data();
		if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF && data[3] == 0xE0) {
			return "image/jpeg";
		}
		else if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
			return "image/png";
		}
	}
	return "application/octet-stream";
}

bool updateTags(char *path, Metadata *meta)
{
    MPEG::File file(path);
	if (!file.isValid() || file.readOnly()) {
		return false;
	}
    ID3v2::Tag *tag = file.ID3v2Tag(true);
	if (!tag) {
		return false;
	}
	if (meta->hasArtist) {
		setTextFrame(tag, "TPE1", meta->artist);
	}
	if (meta->hasAlbum) {
		setTextFrame(tag, "TALB", meta->album);
	}
	if (meta->hasAlbumArtist) {
		setTextFrame(tag, "TPE2", meta->albumartist);
	}
	if (meta->hasTitle) {
		setTextFrame(tag, "TIT2", meta->title);
	}
	if (meta->hasTrackNumber) {
		String text = String::number(meta->trackNumber);
		if (meta->hasTrackCount) {
			text += "/" + String::number(meta->trackCount);
		}
		setTextFrame(tag, "TRCK", text);
	}
	if (meta->hasGenre) {
		setTextFrame(tag, "TCON", meta->genre);
	}
	if (meta->hasPublisher) {
		setTextFrame(tag, "TPUB", meta->publisher);
	}
	if (meta->hasYear) {
		setTextFrame(tag, "TDRC", String::number(meta->year));
	}
	if (meta->hasImage) {
		ID3v2::AttachedPictureFrame *f = new ID3v2::AttachedPictureFrame();
		f->setType(ID3v2::AttachedPictureFrame::Other);
		f->setPicture(meta->image);
		f->setMimeType(detectImageMimeType(meta->image));
		tag->removeFrames("APIC");
		tag->addFrame(f);
	}
	for (Map<String, String>::Iterator it = meta->customTextFrames.begin(); it != meta->customTextFrames.end(); ++it) {
		setTextFrame(tag, it->first.toCString(true), it->second);
	}
	for (Map<String, String>::Iterator it = meta->customUserTextFrames.begin(); it != meta->customUserTextFrames.end(); ++it) {
		setUserTextFrame(tag, it->first, it->second);
	}
	for (Map<String, String>::Iterator it = meta->customUrlFrames.begin(); it != meta->customUrlFrames.end(); ++it) {
		setUrlFrame(tag, it->first.toCString(true), it->second);
	}
	for (Map<String, String>::Iterator it = meta->customUserUrlFrames.begin(); it != meta->customUserUrlFrames.end(); ++it) {
		setUserUrlFrame(tag, it->first, it->second);
	}
	for (Map<String, String>::Iterator it = meta->customUniqueFileIdentifierFrames.begin(); it != meta->customUniqueFileIdentifierFrames.end(); ++it) {
		setUniqueFileIdentifierFrame(tag, it->first, it->second);
	}
	return file.save(MPEG::File::ID3v1 | MPEG::File::ID3v2, false, 3);
}

int main(int argc, char **argv)
{
	int ch;
	Metadata meta;
	while ((ch = getopt(argc, argv, "a:b:A:t:n:N:G:Y:p:i:x:")) != -1) {
		switch (ch) {
		case 'a':
			meta.artist = String(optarg, String::UTF8);
			meta.hasArtist = 1;
			break;
		case 'b':
			meta.albumartist = String(optarg, String::UTF8);
			meta.hasAlbumArtist = 1;
			break;
		case 'A':
			meta.album = String(optarg, String::UTF8);
			meta.hasAlbum = 1;
			break;
		case 't':
			meta.title = String(optarg, String::UTF8);
			meta.hasTitle = 1;
			break;
		case 'n':
			meta.trackNumber = atoi(optarg);
			meta.hasTrackNumber = 1;
			break;
		case 'N':
			meta.trackCount = atoi(optarg);
			meta.hasTrackCount = 1;
			break;
		case 'G':
			meta.genre = String(optarg, String::UTF8);
			meta.hasGenre = 1;
			break;
		case 'p':
			meta.publisher = String(optarg, String::UTF8);
			meta.hasPublisher = 1;
			break;
		case 'Y':
			meta.year = atoi(optarg);
			meta.hasYear = 1;
			break;
		case 'i':
			meta.image = readFile(optarg);
			meta.hasImage = 1;
			break;
		case 'x':
			{
				String tag(optarg, String::UTF8);
				int offset = tag.find("=");
				if (offset == -1) {
					cerr << "invalid tag name " << tag.to8Bit() << endl;
					break;
				}
				String name = tag.substr(0, offset).upper();
				String value = tag.substr(offset + 1);
				if (name.size() != 4) {
					cerr << "invalid tag name " << tag.to8Bit() << endl;
					break;
				}
				cerr << "parsing " << name.to8Bit() << " " << value.to8Bit() << endl;
				if (name == "TXXX" || name == "WXXX" || name == "UFID") {
					offset = value.find("=");
					if (offset == -1) {
						cerr << "invalid tag name " << tag.to8Bit() << endl;
						break;
					}
					String description = value.substr(0, offset);
					value = value.substr(offset + 1);
					if (name == "TXXX") {
						meta.customUserTextFrames[description] = value;
					}
					else if (name == "UFID") {
						meta.customUniqueFileIdentifierFrames[description] = value;
					}
					else {
						meta.customUserUrlFrames[description] = value;
					}
				}
				else if (name[0] == 'T') {
					meta.customTextFrames[name] = value;
				}
				else if (name[0] == 'W') {
					meta.customUrlFrames[name] = value;
				}
			}
			break;
		case '?':
		default:
			usage();
		}
	}

	int exitCode = 0;
	for	(int i = optind; i < argc; i++) {
		if (!updateTags(argv[i], &meta)) {
			cerr << "unable to update file " << argv[i] << endl;
			exitCode = 2;
		}
	}
	return exitCode;
}

