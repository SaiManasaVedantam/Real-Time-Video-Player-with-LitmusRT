# Real-Time Video Player with LitmusRT (April 2022)
This project deals with the implementation of a Real-time Video Player that avoids video lags ensuring high quality service & deals with the common issues faced by using non-Real-time Video applications. It is developed using the real-time interfaces provided by Litmus-RT along with compatible versions of FFMPEG and SDL libraries with the core being built in C language using liblitmus interface provided by Litmus-RT.

## Motivation & Scope
![Motivation](https://user-images.githubusercontent.com/28973352/164765952-be667021-77cc-427d-b7ab-dfe611c85852.JPG)
1. https://www.businessofapps.com/data/video-streaming-app-market/
2. https://www.pewresearch.org/fact-tank/2021/10/27/what-we-know-about-the-increase-in-u-s-murders-in-2020/

## Instructions
To implement the project, we need to install SDL & FFMPEG but LitmusRT does not have enough space to fit the downloaded files & then run real-time tasks. For this, we need to extend the drive space for the LitmusRT instance created on the Virtual Machine. Following the below steps in that order helps to execute the code successfully. 
1. LitmusRT Help manual
https://www.litmus-rt.org/tutor16/manual.pdf
2. Extended drive:
https://linuxhint.com/increase-virtualbox-disk-size/
https://www.yinfor.com/2015/05/virtualbox-resize-hard-disk-error-vbox_e_not_supported.html
3. SDL1.2 installation support:
Sudo apt-get install libsdl1.2–dev
https://linuxfromscratch.org/blfs/view/svn/multimedia/sdl.html
4. FFMPEG support:
http://dranger.com/ffmpeg/end.html
Downloaded ffmpeg-4.1.4-i686-static.tar.xz.md5 file from https://www.johnvansickle.com/ffmpeg/old-releases/
Extracted tar file to /home/litmus/FFMPEG
5. Period - Deadline tradeoff:
https://www.rtx.ece.vt.edu/resources/Files/chantem08jul.pdf


## Tools & Technologies


## Architecture

## Module Information & Workflow

## Evaluation Results (Sample)




