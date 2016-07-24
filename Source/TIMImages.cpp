#include "TIMImages.h"

#include <algorithm>

#define ALPHA_LIMIT 0x60

uint8_t TIMImageDataStruct::color[32] = { 0,  8, 16, 24,
										 32, 40, 48, 56,
										 65, 73, 81, 89,
										100,108,118,125,
										135,143,152,162,
										170,178,186,194,
										202,210,217,225,
										232,240,246,255};

#define MACRO_TIM_IOFUNCTION(IO,SEEK,READ,PPF) \
	unsigned int i; \
	if (PPF) PPFInitScanStep(f); \
	IO ## Long(f,magic_tim); \
	IO ## Long(f,format); \
	if (format & 0x8) { \
		IO ## Long(f,pal_size); \
		IO ## Short(f,pal_x); \
		IO ## Short(f,pal_y); \
		IO ## Short(f,pal_width); \
		IO ## Short(f,pal_height); \
		if (READ) pal_value = new uint8_t[pal_size-0xC]; \
		for (i=0;i+0xC<pal_size;i++) \
			IO ## Char(f,pal_value[i]); \
	} else \
		pal_size = 0; \
	IO ## Long(f,img_size); \
	IO ## Short(f,pos_x); \
	IO ## Short(f,pos_y); \
	IO ## Short(f,width); \
	IO ## Short(f,height); \
	if (READ) pixel_value = new uint8_t[img_size-0xC]; \
	for (i=0;i+0xC<img_size;i++) \
		IO ## Char(f,pixel_value[i]); \
	if (PPF) PPFEndScanStep();
	


void TIMImageDataStruct::Read(fstream& f) {
	if (loaded)
		return;
	if (GetGameType()==GAME_TYPE_PSX) {
		MACRO_TIM_IOFUNCTION(FFIXRead,FFIXSeek,true,false)
	} else {
		unsigned int i,pixelamount;
		f.open(data_file_name.c_str(),ios::in | ios::binary);
		f.seekg(data_file_offset);
		SteamReadLong(f,steam_width);
		SteamReadLong(f,steam_height);
		pixelamount = steam_width*steam_height/16; // each pixel is a 4x4 square with color and alpha variations inside
		SteamReadLong(f,steam_size1);
		SteamReadLong(f,steam_format);
		SteamReadLong(f,(uint32_t&)steam_mip_count);
		SteamReadLong(f,steam_flags);
		SteamReadLong(f,(uint32_t&)steam_image_count);
		SteamReadLong(f,(uint32_t&)steam_dimension);
		SteamReadLong(f,(uint32_t&)steam_filter_mode);
		SteamReadLong(f,(uint32_t&)steam_anisotropic);
		SteamReadLong(f,steam_mip_bias);
		SteamReadLong(f,(uint32_t&)steam_wrap_mode);
		SteamReadLong(f,(uint32_t&)steam_lightmap_format);
		SteamReadLong(f,(uint32_t&)steam_color_space);
		SteamReadLong(f,steam_size2);
		steam_pixel_alpha = new uint8_t[pixelamount];
		steam_pixel_alpha_ex = new uint8_t[pixelamount];
		steam_pixel_alpha_flag1 = new uint16_t[pixelamount];
		steam_pixel_alpha_flag2 = new uint16_t[pixelamount];
		steam_pixel_alpha_flag3 = new uint16_t[pixelamount];
		steam_pixel_color = new uint16_t[pixelamount];
		steam_pixel_color_ex = new uint16_t[pixelamount];
		steam_pixel_color_flag = new uint32_t[pixelamount];
		for (i=0;i<pixelamount;i++) {
			SteamReadChar(f,steam_pixel_alpha[i]);
			SteamReadChar(f,steam_pixel_alpha_ex[i]);
			SteamReadShort(f,steam_pixel_alpha_flag1[i]);
			SteamReadShort(f,steam_pixel_alpha_flag2[i]);
			SteamReadShort(f,steam_pixel_alpha_flag3[i]);
			SteamReadShort(f,steam_pixel_color[i]);
			SteamReadShort(f,steam_pixel_color_ex[i]);
			SteamReadLong(f,steam_pixel_color_flag[i]);
		}
		f.close();
	}
	loaded = true;
}

void TIMImageDataStruct::Write(fstream& f) {
	MACRO_TIM_IOFUNCTION(FFIXWrite,FFIXSeek,false,false)
	modified = false;
}

void TIMImageDataStruct::WritePPF(fstream& f) {
	MACRO_TIM_IOFUNCTION(PPFStepAdd,FFIXSeek,false,true)
}

void TIMImageDataStruct::ReadHWS(fstream& f) {
	MACRO_TIM_IOFUNCTION(HWSRead,HWSSeek,true,false)
	MarkDataModified();
}

void TIMImageDataStruct::WriteHWS(fstream& f) {
	MACRO_TIM_IOFUNCTION(HWSWrite,HWSSeek,false,false)
}

void TIMImageDataStruct::Flush() {
	if (!loaded)
		return;
	if (GetGameType()==GAME_TYPE_PSX) {
		if (format & 0x8)
			delete[] pal_value;
		delete[] pixel_value;
	} else {
		delete[] steam_pixel_alpha;
		delete[] steam_pixel_alpha_ex;
		delete[] steam_pixel_alpha_flag1;
		delete[] steam_pixel_alpha_flag2;
		delete[] steam_pixel_alpha_flag3;
		delete[] steam_pixel_color;
		delete[] steam_pixel_color_ex;
		delete[] steam_pixel_color_flag;
	}
	loaded = false;
}

uint32_t PaletteBuffer[256];
uint32_t ImageBuffer[256*256];

uint16_t TIMImageDataStruct::GetPosX() {
	if (format & 0x8)
		return pos_x*6;
	return pos_x*2;
}

uint16_t TIMImageDataStruct::GetWidth() {
	if (format & 0x8)
		return width*6;
	return width*2;
}

void HandleColorAlpha(uint32_t& color, int alphamode) {
	if (alphamode==1) {
		if ((color & 0xFFFFFF)==0)
			color = 0;
		else
			color |= 0xFF000000;
	}
}

uint32_t* TIMImageDataStruct::ConvertAsPalette(uint16_t palpos, bool usealpha) {
	int i;
	uint32_t* res = PaletteBuffer;
	uint8_t pb = 2;//format & 0x3;
	uint32_t paltmp;
	if (format & 0x8) {
		uint32_t paltmp;
		for (i=0;i<pal_width*pal_height;i++) {
			paltmp = pal_value[(palpos*pal_width+i)*pb];
			paltmp |= pal_value[(palpos*pal_width+i)*pb+1] << 8;
			uint32_t r = color[paltmp & 0x1F];
			uint32_t g = color[(paltmp >> 5) & 0x1F];
			uint32_t b = color[(paltmp >> 10) & 0x1F];
			res[i] = (r << 16) | (g << 8) | b;
			if (paltmp & 0x8000)
				res[i] |= 0xFF000000;
			HandleColorAlpha(res[i],usealpha ? 0 : 1);
		}
		while (i<256) {
			res[i] = res[i-pal_width];
			i++;
		}
	} else {
		uint16_t palx = (palpos & 0x3F)*32;
		uint16_t paly = palpos >> 6;
		if (pos_x>palx || pos_x+width<palx+256 || pos_y>paly || pos_y+height<=paly)
			return NULL;
		palx -= pos_x;
		paly -= pos_y;
		for (i=0;i<256;i++) {
			paltmp = pixel_value[(palx+i+paly*width)*pb];
			paltmp |= pixel_value[(palx+i+paly*width)*pb+1] << 8;
			uint32_t r = color[paltmp & 0x1F];
			uint32_t g = color[(paltmp >> 5) & 0x1F];
			uint32_t b = color[(paltmp >> 10) & 0x1F];
			res[i] = (r << 16) | (g << 8) | b;
			if (paltmp & 0x8000)
				res[i] |= 0xFF000000;
			HandleColorAlpha(res[i],usealpha ? 0 : 1);
		}
	}
	return res;
}

uint32_t* TIMImageDataStruct::ConvertAsImage(uint16_t texpos, uint16_t sizex, uint16_t sizey, uint32_t* pal, uint16_t palpos, bool usealpha) {
	unsigned int i,j;
	uint32_t alphamask = usealpha ? 0 : 0xFF000000;
	if (pal==NULL) {
		if (format & 0x8)
			pal = ConvertAsPalette(palpos,usealpha);
		else
			return NULL;
	}
	uint32_t* res = ImageBuffer;
	uint32_t texx, texy;
	uint8_t pb = 2;//format & 0x3;
	if (format & 0x8) {
		texx = texpos & 0xFF;
		texy = texpos >> 8;
		sizex /= 3;
		for (i=0;i<sizex;i++)
			for (j=0;j<sizey;j++) {
				uint8_t first = pixel_value[i+texx+(j+texy)*width*pb] & 0xF;
				uint8_t last = pixel_value[i+texx+(j+texy)*width*pb] >> 4;
				uint32_t midcol = ComputeMidColor(pal[first],pal[last]);
				res[(i+j*sizex)*3] = pal[first] | alphamask;
				res[(i+j*sizex)*3+1] = midcol | alphamask;
				res[(i+j*sizex)*3+2] = pal[last] | alphamask;
			}
	} else {
		texx = (texpos & 0xF)*64-pos_x;
		texy = ((texpos & 0x10) >> 4)-pos_y;
		for (i=0;i<sizex;i++)
			for (j=0;j<sizey;j++)
				res[i+j*sizex] = pal[pixel_value[i+texx*pb+(j+texy)*width*pb]] | alphamask;
	}
	return res;
}

uint32_t* TIMImageDataStruct::ConvertAsSteamImage(bool usealpha) {
	unsigned int i,j,x,y,k,l;
	uint8_t r,g,b,a,colorflag;
	uint64_t alphaflag;
	uint32_t* res = new uint32_t[steam_width*steam_height];
	a = 0xFF;
	i = 0;
	j = steam_width*steam_height-steam_width;
	for (y=0;y<steam_height/4;y++) {
		for (k=0;k<4;k++) {
			for (x=0;x<steam_width/4;x++) {
				for (l=0;l<4;l++) {
					colorflag = ((steam_pixel_color_flag[i] >> 2*(4*k+l)) & 3);
					alphaflag = steam_pixel_alpha_flag1[i] | (steam_pixel_alpha_flag2[i] << 16) | (steam_pixel_alpha_flag2[i] << 32);
					alphaflag = ((alphaflag >> 3*(4*k+l)) & 7);
					if (colorflag==0) {
						r = (steam_pixel_color[i] >> 11) & 0x1F;
						g = (steam_pixel_color[i] >> 6) & 0x1F;
						b = steam_pixel_color[i] & 0x1F;
					} else if (colorflag==1) {
						r = (steam_pixel_color_ex[i] >> 11) & 0x1F;
						g = (steam_pixel_color_ex[i] >> 6) & 0x1F;
						b = steam_pixel_color_ex[i] & 0x1F;
					} else if (colorflag==2) {
						r = (2*((steam_pixel_color[i] >> 11) & 0x1F)+((steam_pixel_color_ex[i] >> 11) & 0x1F))/3;
						g = (2*((steam_pixel_color[i] >> 6) & 0x1F)+((steam_pixel_color_ex[i] >> 6) & 0x1F))/3;
						b = (2*(steam_pixel_color[i] & 0x1F)+(steam_pixel_color_ex[i] & 0x1F))/3;
					} else {
						r = (((steam_pixel_color[i] >> 11) & 0x1F)+2*((steam_pixel_color_ex[i] >> 11) & 0x1F))/3;
						g = (((steam_pixel_color[i] >> 6) & 0x1F)+2*((steam_pixel_color_ex[i] >> 6) & 0x1F))/3;
						b = ((steam_pixel_color[i] & 0x1F)+2*(steam_pixel_color_ex[i] & 0x1F))/3;
					}
					if (usealpha) {
						if (alphaflag==0) {
							a = steam_pixel_alpha[i];
						} else if (alphaflag==1) {
							a = steam_pixel_alpha_ex[i];
						} else {
							if (alphaflag==6 && steam_pixel_alpha[i]<=steam_pixel_alpha_ex[i]) {
								a = 0;
							} else if (alphaflag==7 && steam_pixel_alpha[i]<=steam_pixel_alpha_ex[i]) {
								a = 0xFF;
							} else if (steam_pixel_alpha[i]<=steam_pixel_alpha_ex[i]) {
								a = ((6-alphaflag)*steam_pixel_alpha[i]+(alphaflag-1)*steam_pixel_alpha_ex[i])/5;
							} else {
								a = ((8-alphaflag)*steam_pixel_alpha[i]+(alphaflag-1)*steam_pixel_alpha_ex[i])/7;
							}
						}
					}
					res[j++] = (a << 24) | (color[r] << 16) | (color[g] << 8) | color[b];
				}
				i++;
			}
			j -= 2*steam_width;
			i -= steam_width/4;
		}
		i += steam_width/4;
	}
/*fstream ftga("aaaa.tga",ios::out|ios::binary);
if (!ftga.is_open()) return res;
uint32_t tmp = 0x20000;
ftga.write((const char*)&tmp,4);
tmp = 0;
ftga.write((const char*)&tmp,4);
ftga.write((const char*)&tmp,4);
tmp = steam_width;
ftga.write((const char*)&tmp,2);
tmp = steam_height;
ftga.write((const char*)&tmp,2);
tmp = 0x2020;
ftga.write((const char*)&tmp,2);
tmp = 0xFF;
for (i=0;i<steam_height*steam_width;i++) {
a = (res[i] >> 24) & 0xFF; r = (res[i] >> 16) & 0xFF; g = (res[i] >> 8) & 0xFF; b = res[i] & 0xFF;
ftga.write((const char*)&b,1);
ftga.write((const char*)&g,1);
ftga.write((const char*)&r,1);
ftga.write((const char*)&a,1);
}*/
	return res;
}

uint32_t* TIMImageDataStruct::ConvertAsFullImage(uint32_t* pal, uint16_t palpos, bool usealpha) {
	unsigned int i,j;
	uint32_t alphamask = usealpha ? 0 : 0xFF000000;
	uint8_t pb = 2;//format & 0x3;
	if (pal==NULL) {
		if (format & 0x8)
			pal = ConvertAsPalette(palpos,usealpha);
		else
			return NULL;
	}
	uint32_t* res = new uint32_t[GetWidth()*height];
	if (format & 0x8) {
		for (i=0;i<width*pb;i++)
			for (j=0;j<height;j++) {
				uint8_t first = pixel_value[i+j*width*pb] & 0xF;
				uint8_t last = pixel_value[i+j*width*pb] >> 4;
				uint32_t midcol = ComputeMidColor(pal[first],pal[last]);
				res[(i+j*width*pb)*3] = pal[first] | alphamask;
				res[(i+j*width*pb)*3+1] = midcol | alphamask;
				res[(i+j*width*pb)*3+2] = pal[last] | alphamask;
			}
	} else {
		for (i=0;i<GetWidth();i++)
			for (j=0;j<height;j++)
				res[i+j*GetWidth()] = pal[pixel_value[i+j*GetWidth()]] | alphamask;
	}
	return res;
}

int TIMImageDataStruct::Export(const char* outputfile, bool full, uint16_t texpos, uint32_t* pal, uint16_t palpos, bool usealpha) {
	uint16_t expwidth = 256;
	uint16_t expheight = 256;
	if (pal==NULL) {
		if (format & 0x8)
			pal = ConvertAsPalette(palpos,usealpha);
		else
			return 2;
		expwidth = GetWidth();
		expheight = height;
	}
	fstream ftga(outputfile,ios::out|ios::binary);
	if (!ftga.is_open())
		return 1;
	uint32_t* packed_data;
	if (full)
		packed_data = ConvertAsFullImage(pal,usealpha);
	else
		packed_data = ConvertAsImage(texpos,256,256,pal,usealpha);
	uint32_t tmp = 0x20000;
	ftga.write((const char*)&tmp,4);
	tmp = 0;
	ftga.write((const char*)&tmp,4);
	ftga.write((const char*)&tmp,4);
	ftga.write((const char*)&expwidth,2);
	ftga.write((const char*)&expheight,2);
	tmp = 0x2020;
	ftga.write((const char*)&tmp,2);
	ftga.write((const char*)packed_data,expwidth*expheight*4);
	ftga.close();
	if (full)
		delete[] packed_data;
	return 0;
}

uint32_t SearchBestColor(uint32_t color, uint32_t* pal, uint32_t palsize, int transpalbyte) {
	uint8_t red = color & 0xFF;
	uint8_t green = (color >> 8) & 0xFF;
	uint8_t blue = (color >> 16) & 0xFF;
	uint8_t alpha = color >> 24;
	if (alpha<=ALPHA_LIMIT && transpalbyte>=0)
		return transpalbyte;
	uint32_t err,tmperr;
	uint32_t res = 0;
	if ((pal[0] >> 24)<=ALPHA_LIMIT)
		err = 1000;
	else
		err = abs(red-(int)((pal[0] >> 16) & 0xFF))/*
			*/+abs(green-(int)((pal[0] >> 8) & 0xFF))/*
			*/+abs(blue-(int)(pal[0] & 0xFF));
	for (unsigned int i=1;i<palsize;i++) {
		if ((pal[i] >> 24)<=ALPHA_LIMIT)
			tmperr = 1000;
		else
			tmperr = abs(red-(int)((pal[i] >> 16) & 0xFF))/*
					*/+abs(green-(int)((pal[i] >> 8) & 0xFF))/*
					*/+abs(blue-(int)(pal[i] & 0xFF));
		if (tmperr<err) {
			err = tmperr;
			res = i;
		}
	}
	return res;
}

void TIMImageDataStruct::Import(uint8_t* colordata, uint8_t* alphadata, uint16_t posx, uint16_t posy, uint16_t sizex, uint16_t sizey, uint32_t* pal, int charflag) {
	unsigned int i,j,index;
	int transpalbyte = -1;
	for (i=0;i<256;i++)
		if (pal[i] >> 24<=ALPHA_LIMIT) {
			transpalbyte = i;
			break;
		}
	uint8_t pb = 2;//format & 0x3;
	if (format & 0x8) {
		uint8_t palbyte1,palbyte2;
		posx /= 3;
		for (i=0;i<sizex/3;i++)
			for (j=0;j<sizey;j++) {
				index = 3*i+j*sizex;
				palbyte1 = SearchBestColor(colordata[3*index] | (colordata[3*index+1] << 8) | (colordata[3*index+2] << 16) | (alphadata[index] << 24),pal,256,transpalbyte);
				palbyte2 = SearchBestColor(colordata[3*(index+2)] | (colordata[3*(index+2)+1] << 8) | (colordata[3*(index+2)+2] << 16) | (alphadata[index+2] << 24),pal,256,transpalbyte);
				if (charflag==0)
					pixel_value[i+posx+(j+posy)*width*pb] = (palbyte1 & 0x3) | ((palbyte2 & 0x3) << 4) | (pixel_value[i+posx+(j+posy)*width*pb] & 0xCC);
				else if (charflag==1)
					pixel_value[i+posx+(j+posy)*width*pb] = (palbyte1 & 0xC) | ((palbyte2 & 0xC) << 4) | (pixel_value[i+posx+(j+posy)*width*pb] & 0x33);
				else
					pixel_value[i+posx+(j+posy)*width*pb] = (palbyte1 & 0xF) | ((palbyte2 & 0xF) << 4);
			}
	} else {
		for (i=0;i<sizex;i++)
			for (j=0;j<sizey;j++) {
				index = i+j*sizex;
				pixel_value[i+posx+(j+posy)*width*pb] = SearchBestColor(colordata[3*index] | (colordata[3*index+1] << 8) | (colordata[3*index+2] << 16) | (alphadata[index] << 24),pal,256,transpalbyte);
			}
	}
}

void TIMImageDataStruct::ImportPalette(uint16_t amount, uint32_t** paldata) {
	unsigned int i,j;
	uint8_t alpha,red,green,blue;
	uint8_t pb = 2;//format & 0x3;
	uint32_t* s = &img_size;
	uint16_t* h = &height;
	uint8_t** v = &pixel_value;
	if (format & 0x8) {
		s = &pal_size;
		h = &pal_height;
		v = &pal_value;
	}
	uint8_t*& val = *v;
	if (amount!=*h) {
		delete[] val;
		val = new uint8_t[amount*width*pb];
		*h = amount;
		*s = amount*width*pb+0xC;
		SetSize(0x8+pal_size+img_size);
	}
	for (j=0;j<amount;j++)
		for (i=0;i<256;i++) {
			alpha = paldata[j][i] >> 24;
			red = (paldata[j][i] >> 16) & 0xFF;
			green = (paldata[j][i] >> 8) & 0xFF;
			blue = paldata[j][i] & 0xFF;
			val[(i+j*width)*pb] = (red >> 3) | ((green >> 3) << 5);
			val[(i+j*width)*pb+1] = (green >> 6) | ((blue >> 3) << 2);
			if (alpha>ALPHA_LIMIT)
				pixel_value[(i+j*width)*pb+1] |= 0x80;
		}
}

void TIMImageDataStruct::SetPixelValue(uint16_t x, uint16_t y, uint8_t pxvalue, int charflag) {
	uint8_t pb = 2;//format & 0x3;
	if (format & 0x8) {
		unsigned int index = x/3+y*width*pb;
		if (charflag==0) {
			if (x%3<=1)
				pixel_value[index] = (pixel_value[index] & 0xFC) | (pxvalue & 0x3);
			else if (x%3==2)
				pixel_value[index] = (pixel_value[index] & 0xCF) | ((pxvalue & 0x3) << 4);
		} else if (charflag==1) {
			if (x%3<=1)
				pixel_value[index] = (pixel_value[index] & 0xF3) | ((pxvalue & 0x3) << 2);
			else if (x%3==2)
				pixel_value[index] = (pixel_value[index] & 0x3F) | ((pxvalue & 0x3) << 6);
		} else {
			if (x%3<=1)
				pixel_value[index] = (pixel_value[index] & 0xF0) | (pxvalue & 0xF);
			else if (x%3==2)
				pixel_value[index] = (pixel_value[index] & 0xF) | ((pxvalue & 0xF) << 4);
		}
	} else {
		pixel_value[x+y*width*pb] = pxvalue;
	}
}

uint8_t vram[1024][512][2];
void TIMImageDataStruct::LoadInVRam(bool loadallchunk) {
	TIMImageDataStruct* tim;
	unsigned int i,x,y,k;
	for (i=0;(loadallchunk && i<parent_chunk->object_amount) || (!loadallchunk && i==0);i++) {
		tim = loadallchunk ? &(TIMImageDataStruct&)parent_chunk->GetObject(i) : this;
		if (tim->format & 0x8)
			for (x=0;x<tim->pal_width;x++)
				for (y=0;y<tim->pal_height;y++)
					for (k=0;k<2;k++)
						vram[tim->pal_x+x][tim->pal_y+y][k] = tim->pal_value[(x+y*tim->pal_width)*2+k];
		for (x=0;x<tim->width;x++)
			for (y=0;y<tim->height;y++)
				for (k=0;k<2;k++)
					vram[tim->pos_x+x][tim->pos_y+y][k] = tim->pixel_value[(x+y*tim->width)*2+k];
	}
/*fstream ftga("aaaa.tga",ios::out|ios::binary);
if (!ftga.is_open()) return;
uint32_t tmp = 0x20000;
ftga.write((const char*)&tmp,4);
tmp = 0;
ftga.write((const char*)&tmp,4);
ftga.write((const char*)&tmp,4);
tmp = steam_width;
ftga.write((const char*)&tmp,2);
tmp = steam_height;
ftga.write((const char*)&tmp,2);
tmp = 0x2020;
ftga.write((const char*)&tmp,2);
tmp = 0xFF;
i = 0;
unsigned int l;
uint8_t checkflag;
for (y=0;y<steam_height/4;y++) {for (k=0;k<4;k++) {for (x=0;x<steam_width/4;x++) { for (l=0;l<4;l++) {
r1 = (steam_pixel_color[i] >> 11) & 0x1F; r2 = (steam_pixel_color_ex[i] >> 11) & 0x1F;
g1 = (steam_pixel_color[i] >> 6) & 0x1F; g2 = (steam_pixel_color_ex[i] >> 6) & 0x1F;
b1 = steam_pixel_color[i] & 0x1F; b2 = steam_pixel_color_ex[i] & 0x1F;
checkflag = ((steam_pixel_color_flag[i] >> 2*(4*k+l)) & 3);
if (checkflag==0) { }
else if (checkflag==1) { r1 = r2; g1 = g2; b1 = b2; }
else if (checkflag==2) { r1 = (2*r1+r2)/3; g1 = (2*g1+g2)/3; b1 = (2*b1+b2)/3; }
else if (checkflag==3) { r1 = (r1+2*r2)/3; g1 = (g1+2*g2)/3; b1 = (b1+2*b2)/3; }
ftga.write((const char*)&color[b1],1);
ftga.write((const char*)&color[g1],1);
ftga.write((const char*)&color[r1],1);
ftga.write((const char*)&tmp,1);
} i++;} i -= steam_width/4;} i += steam_width/4;}*/
//for (y=0;y<512;y++) for (x=0;x<1024;x++) for (k=0;k<1;k++) {
//for (i=0;i<3;i++) ftga.write((const char*)&vram[x][y][k],1); ftga.write((const char*)&tmp,1);}
}

uint32_t TIMImageDataStruct::GetVRamPixel(unsigned int x, unsigned int y, unsigned int px, unsigned int py, bool shortformat) {
	uint8_t pal;
	if (shortformat) {
		pal = vram[x/4][y][x&2];
		if (x&1)
			pal = (pal & 0xF0) >> 4;
		else
			pal &= 0x0F;
	} else {
		pal = vram[x/2][y][x&1];
	}
	uint16_t pix = vram[px+pal][py][0] | (vram[px+pal][py][1] << 8);
	uint8_t r = color[pix & 0x1F];
	uint8_t g = color[(pix >> 5) & 0x1F];
	uint8_t b = color[(pix >> 10) & 0x1F];
	uint32_t res = (r << 16) | (g << 8) | b;
	if (pix & 0x8000)
		res |= 0xFF000000;
	return res;
}

uint32_t TIMImageDataStruct::ComputeMidColor(uint32_t leftcolor, uint32_t rightcolor) {
	uint32_t res;
	if ((leftcolor >> 24)==0)
		res = rightcolor;
	else if ((rightcolor >> 24)==0)
		res = leftcolor;
	else {
		res = ((leftcolor & 0xFF)+(rightcolor & 0xFF))/2;
		res |= ((((leftcolor >> 8) & 0xFF)+((rightcolor >> 8) & 0xFF))/2) << 8;
		res |= ((((leftcolor >> 16) & 0xFF)+((rightcolor >> 16) & 0xFF))/2) << 16;
		res |= 0xFF000000;
	}
	return res;
}

uint16_t colorcount[32][32][32];
uint32_t* TIMImageDataStruct::CreatePaletteFromData(uint8_t* colordata, uint8_t* alphadata, uint16_t sizex, uint16_t sizey) {
	uint32_t* res = new uint32_t[256];
	unsigned int i,j,k,index;
	unsigned int numcolor = 0;
	unsigned int maxcoloramount = 0;
	bool usetrans = false;
	uint8_t red,green,blue;
	for (i=0;i<32;i++)
		for (j=0;j<32;j++)
			for (k=0;k<32;k++)
				colorcount[i][j][k] = 0;
	for (i=0;i<sizex;i++)
		for (j=0;j<sizey;j++) {
			index = i+j*sizex;
			if (alphadata[index]<=ALPHA_LIMIT) {
				if (!usetrans) {
					usetrans = true;
					numcolor++;
				}
			} else {
				index *= 3;
				red = (colordata[index+2]+4)/8;
				green = (colordata[index+1]+4)/8;
				blue = (colordata[index]+4)/8;
				if (colorcount[red][green][blue]==0)
					numcolor++;
				colorcount[red][green][blue]++;
				maxcoloramount = max(maxcoloramount,(unsigned int)colorcount[red][green][blue]);
			}
		}
	index = 0;
	if (numcolor<=256) {
		if (usetrans)
			res[index++] = 0;
		for (i=0;i<32;i++)
			for (j=0;j<32;j++)
				for (k=0;k<32;k++)
					if (colorcount[i][j][k]>0)
						res[index++] = color[i] | (color[j] << 8) | (color[k] << 16) | 0xFF000000;
		while (index<256)
			res[index++] = 0;
	} else {
		uint8_t palred[256],palgreen[256],palblue[256];
		int pali,palj,palamount = 1;
		palred[0] = 0;
		palgreen[0] = 0;
		palblue[0] = 0;
		for (i=0;i<32;i++)
			for (j=0;j<32;j++)
				for (k=0;k<32;k++)
					for (pali=0;pali<palamount;pali++)
						if (colorcount[i][j][k]<colorcount[palred[pali]][palgreen[pali]][palblue[pali]]) {
							if (pali==0) {
								if (palamount<256) { // palette not filled + new min
									for (palj=palamount-1;palj>=0;palj--) {
										palred[palj+1] = palred[palj];
										palgreen[palj+1] = palgreen[palj];
										palblue[palj+1] = palblue[palj];
									}
									palred[0] = i;
									palgreen[0] = j;
									palblue[0] = k;
									palamount++;
									break;
								}
							} else {
								if (palamount<256) { // palette not filled + new element
									for (palj=palamount-1;palj>=pali;palj--) {
										palred[palj+1] = palred[palj];
										palgreen[palj+1] = palgreen[palj];
										palblue[palj+1] = palblue[palj];
									}
									palred[pali] = i;
									palgreen[pali] = j;
									palblue[pali] = k;
									palamount++;
									break;
								} else { // palette already filled + new element
									for (palj=1;palj<=pali;palj++) {
										palred[palj-1] = palred[palj];
										palgreen[palj-1] = palgreen[palj];
										palblue[palj-1] = palblue[palj];
									}
									palred[pali] = i;
									palgreen[pali] = j;
									palblue[pali] = k;
									break;
								}
							}
						} else if (pali==palamount-1) {
							if (palamount<256) { // palette not filled + new max
								palred[palamount] = i;
								palgreen[palamount] = j;
								palblue[palamount] = k;
								palamount++;
								break;
							} else { // palette already filled + new max
								for (palj=1;palj<=255;palj++) {
									palred[palj-1] = palred[palj];
									palgreen[palj-1] = palgreen[palj];
									palblue[palj-1] = palblue[palj];
								}
								palred[255] = i;
								palgreen[255] = j;
								palblue[255] = k;
								break;
							}
						}
		unsigned int liminf = 0;
		if (usetrans) {
			res[0] = 0;
			liminf = 1;
		}
		i = 0;
		index = 255;
		while (index>=liminf) {
			res[index--] = color[palred[i]] | (color[palgreen[i]] << 8) | (color[palblue[i]] << 16) | 0xFF000000;
			i++;
		}
	}
	return res;
}

TIMImageDataStruct& TIMImageDataStruct::GetTIMPaletteStruct(TIMImageDataStruct& tim1, TIMImageDataStruct& tim2) {
	// To be improved
	if (tim1.height<256)
		return tim1;
	return tim2;
}

TIMImageDataStruct& TIMImageDataStruct::GetTIMTextureStruct(TIMImageDataStruct& tim1, TIMImageDataStruct& tim2) {
	// To be improved
	if (tim1.height<256)
		return tim2;
	return tim1;
}

uint32_t ImageMergePixels(uint32_t pix1, uint32_t pix2, TIM_BlendMode mode) {
	uint8_t r1, r2, rr;
	uint8_t g1, g2, gr;
	uint8_t b1, b2, br;
	uint8_t a1, a2, ar;
	a1 = (pix1 >> 24) & 0xFF;	a2 = (pix2 >> 24) & 0xFF;
	r1 = (pix1 & 0xFF)*a1/255;	g1 = ((pix1 >> 8) & 0xFF)*a1/255;	b1 = ((pix1 >> 16) & 0xFF)*a1/255;
	r2 = (pix2 & 0xFF)*a2/255;	g2 = ((pix2 >> 8) & 0xFF)*a2/255;	b2 = ((pix2 >> 16) & 0xFF)*a2/255;
	if (a1==0 && a2==0)
		return 0;
	// ToDo : needs more work
	switch (mode) {
	case TIM_BLENDMODE_ALPHA:
		ar = a1+a2-a1*a2/255;
		rr = min(255,r2+(255-a2)*r1/255);	gr = min(255,g2+(255-a2)*g1/255);	br = min(255,b2+(255-a2)*b1/255);
		break;
	case TIM_BLENDMODE_LIGHT:
		ar = max(a1,a2);
		rr = min(255,r1+r2);				gr = min(255,g1+g2);				br = min(255,b1+b2);
		break;
	case TIM_BLENDMODE_SHADE:
		ar = max(a1,a2);
		rr = (a1*r1+a2*r2)/(a1+a2);		gr = (a1*g1+a2*g2)/(a1+a2);		br = (a1*b1+a2*b2)/(a1+a2);
		break;
	default:
		rr = r2;	gr = g2;	br = b2;	ar = a2;
	}
	return rr | (gr << 8) | (br << 16) | (ar << 24);
}
