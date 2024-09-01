# raf-thumbnailer

A fast thumbnailer for RAF image files containing raw images.

## Description

Creates thumbnails from RAF files.

It is very fast, because it will extract the existing thumbnail that is embedded into the JPEG that is embedded into the RAF.
I see it generate a PNG thumbnail from a 50 MegaPixel raw photo in 10 miliseconds or so.

It can write out the thumbnail either as JFIF or as PNG.

## Usage

To extract a thumbnail image:

```
$ raf-thumbnailer IMAGE.RAF IMAGE.PNG
```

## Known issues

I have tested these against RAF output from a Fujifilm GFX50R where the RAF files had JPEG embedded.

This still needs testing against variants from other manufacturers, and against RAF that has no embedded JPEG data.

## Dependencies

The raf-thumbnailer program depends on STB's image and write image libraries, both in the public domain.

## Author

[raf-thumbnailer](github.com:stolk/raf-thumbnailer.git) is by Bram Stolk (b.stolk@gmail.com)

## License

GPLv3


