#include "TXPK/Core/Texture.hpp"

#include <iostream>
#include <filesystem>

#include "IL/ilu.h"

namespace txpk
{
	Texture::Texture()
	{
		path = std::string();
		bounds = NULL;
		offsetX = 0;
		offsetY = 0;
		sourceWidth = 0;
		sourceHeight = 0;
		raw = std::vector<Color4>();
	}

	Texture::Texture(const Texture& other)
	{
		path = other.path;
		bounds = other.bounds;
		offsetX = other.offsetX;
		offsetY = other.offsetY;
		sourceWidth = other.sourceWidth;
		sourceHeight = other.sourceHeight;
		raw = other.raw;
	}

	bool Texture::loadFromIL(uint pIlID)
	{
		ilBindImage(pIlID);
		ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

		auto width = ilGetInteger(IL_IMAGE_WIDTH);
		auto height = ilGetInteger(IL_IMAGE_HEIGHT);

		const std::size_t size = width * height;
		raw.resize(size);

	    ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, raw.data());

		this->sourceWidth = width;
		this->sourceHeight = height;
		this->bounds = std::make_shared<Rectangle>(Rectangle(width, height));
		this->path = std::string();

		return true;
	}

	bool Texture::loadFromMemory(const Color4* data, uint32 width, uint32 height)
	{
		const uint32 size = width * height;
		raw.resize(0);
		raw.resize(size);

		memcpy(&raw[0], data, sizeof(Color4) * size);
		
		this->sourceWidth = width;
		this->sourceHeight = height;
		this->bounds = std::make_shared<Rectangle>(Rectangle(width, height));
		this->path = std::string();

		return true;
	}

	bool Texture::save(const std::string& path)
	{
		auto rawRGBASize = sourceWidth * sourceHeight;
		if (raw.size() < rawRGBASize) {
			std::cout << "Not enough data in the raw buffer, buffer: '" << raw.size() << "' calc: '" << rawRGBASize << "'" << std::endl;
			return false;
		}

		std::uint32_t ilid;
		ilGenImages(1, &ilid);
		ilBindImage(ilid);

		// Create the Image
		ilClearColor(0.0F, 0.0F, 0.0F, 1.0F);
		if (ilTexImage(sourceWidth, sourceHeight, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, nullptr) != IL_TRUE) {
			auto errno_ = ilGetError();
			auto error = iluErrorString(errno_);
			std::cout << "Failed to create an image: " << error << std::endl;
			ilDeleteImage(ilid);
			return false;
		}

		// Convert the data to a plain uint8_t array.
		auto ptr = reinterpret_cast<std::uint8_t*>(&raw[0]);
		auto buffer = std::vector<std::uint8_t>(ptr, ptr + raw.size()*4);
		std::cout << "'" << buffer.size() << "', '" << rawRGBASize*4 << "'" << std::endl;

		// Copy the data from the buffer to the image.
		memcpy(ilGetData(), buffer.data(), buffer.size());

		ilEnable(IL_FILE_OVERWRITE);
		if (std::filesystem::path(path).extension() == ".dds") {
    		ilSetInteger(IL_DXTC_FORMAT, IL_DXT5);
    		ilEnable(IL_NVIDIA_COMPRESS);
  		}

  		if (ilSaveImage(static_cast<const ILstring>(path.c_str())) != IL_TRUE) {
			std::cout << "Failed to save an image." << std::endl;
			ilDeleteImage(ilid);
			return false;
  		}

		ilDeleteImage(ilid);
  		return true;
	}

	void Texture::trim()
	{
		Margin margin;
		bool colorFound = false;
		
		//iterating top row
		for (uint32 y = 0; y < bounds->height && !colorFound; ++y)
		{
			for (uint32 x = 0; x < bounds->width && !colorFound; ++x)
			{
				if (raw[y * bounds->width + x].data[3] != 0)
					colorFound = true;
			}

			if (colorFound == false)
				margin.top++;
		}

		//iterating left columns
		colorFound = false;
		for (uint32 x = 0; x < bounds->width && !colorFound; ++x)
		{
			for (uint32 y = 0; y < bounds->height && !colorFound; ++y)
			{
				if (raw[y * bounds->width + x].data[3] != 0)
					colorFound = true;
			}

			if (colorFound == false)
				margin.left++;
		}

		//iterating bottom rows
		colorFound = false;
		for (uint32 y = bounds->height - 1; y > 0 && !colorFound; --y)
		{
			for (uint32 x = 0; x < bounds->width && !colorFound; ++x)
			{
				if (raw[y * bounds->width + x].data[3] != 0)
					colorFound = true;
			}

			if (colorFound == false)
				margin.bottom++;
		}

		//iterating right columns
		colorFound = false;
		for (uint32 x = bounds->width - 1; x > 0 && !colorFound; --x)
		{
			for (uint32 y = 0; y < bounds->height && !colorFound; ++y)
			{
				if (raw[y * bounds->width + x].data[3] != 0)
					colorFound = true;
			}

			if (colorFound == false)
				margin.right++;
		}

		/*
		 leftOffset  rightOffset
		     |         |
		   _______________ 
		  |   _________	  | __ topOffset
		  |  |xxxxxxxxx|  |
		  |  |xxxxxxxxx|  |
		  |  |xxxxxxxxx|  |
		  |  |xxxxxxxxx|  | __ bottomOffset
		  |_______________|
		
		*/

		this->bounds->width = sourceWidth - margin.left - margin.right;
		this->bounds->height = sourceHeight - margin.top - margin.bottom;
		this->offsetX = margin.left;
		this->offsetY = margin.top;

		raw = apply_margin(raw, sourceWidth, sourceHeight, margin);
	}

	void Texture::adjustRotation()
	{
		//let's play some tetris
		//https://github.com/Seng3694/Tetris/blob/master/engine/src/tetromino.c#L117

		if (bounds->isRotated())
		{
			/*
			assume this texture

			0 1 2 3 4 5
			6 7 8 9 A B
			C D E F G H

			layed out in memory like this  0 1 2 3 4 5 6 7 8 9 A B C D E F G H
			has to be transformed to this (90deg ccw)

			5 B H
			4 A G
			3 9 F
			2 8 E
			1 7 D
			0 6 C

			layed out in memory like this 5 B H 4 A G 3 9 F 2 8 E 1 7 D 0 6 C
			a simple memcpy won't be enough. values can't even be swapped. so we need a copy

			array has still the same size, so no resizing required
			*/

			std::vector<Color4> copy(raw.begin(), raw.end());

			for (uint32 y = 0; y < bounds->width; ++y)
			{
				for (uint32 x = 0; x < bounds->height; ++x)
				{
					raw[(bounds->height - x - 1) * bounds->width + y] = copy[(y * bounds->height) + x];
				}
			}
		}
	}

	bool Texture::operator ==(const Texture& other) const
	{
		if (this->bounds->width != other.bounds->width
			|| this->bounds->height != other.bounds->height)
			return false;

		return array_equals(&this->raw[0], &other.raw[0], static_cast<uint32>(this->raw.size()));
	}

	bool Texture::operator !=(const Texture& other) const
	{
		return !(*this == other);
	}
}
