# Real-Time Video Player with LitmusRT (April 2022)
This project deals with the implementation of a Real-time Video Player that avoids video lags ensuring high quality service & deals with the common issues faced by using non-Real-time Video applications. It is developed using the real-time interfaces provided by Litmus-RT along with compatible versions of FFMPEG and SDL libraries with the core being built in C language using liblitmus interface provided by Litmus-RT.

## Motivation & Scope
![Motivation](https://user-images.githubusercontent.com/28973352/164765952-be667021-77cc-427d-b7ab-dfe611c85852.JPG)
1. https://www.businessofapps.com/data/video-streaming-app-market/
2. https://www.pewresearch.org/fact-tank/2021/10/27/what-we-know-about-the-increase-in-u-s-murders-in-2020/

## Tools & Technologies
<img width="845" alt="Screen Shot 2022-05-17 at 6 38 23 PM" src="https://user-images.githubusercontent.com/28973352/168930459-bdb28fe6-3dd1-44d7-8030-587703cad62a.png">

## Architecture
<img width="650" alt="Screen Shot 2022-05-17 at 6 41 55 PM" src="https://user-images.githubusercontent.com/28973352/168930491-4b71e52a-f7d7-44e9-ad8e-25d05eb19743.png">

## Module Information & Workflow
<img width="570" alt="Screen Shot 2022-05-17 at 6 41 27 PM" src="https://user-images.githubusercontent.com/28973352/168930531-e02976fd-0f47-4c95-b47c-f67e6878c714.png">
<img width="650" alt="Screen Shot 2022-05-17 at 6 41 45 PM" src="https://user-images.githubusercontent.com/28973352/168930542-4349821d-0f1c-4751-930f-2d5225e113ce.png">

## Setup Instructions
To implement the project, we need to install SDL & FFMPEG but LitmusRT does not have enough space to fit the downloaded files & then run real-time tasks. For this, we need to extend the drive space for the LitmusRT instance created on the Virtual Machine. Following the below steps in that order helps to execute the code successfully. 
1. LitmusRT Help manual
- https://www.litmus-rt.org/tutor16/manual.pdf
2. Extended drive:
- https://linuxhint.com/increase-virtualbox-disk-size/
- https://www.yinfor.com/2015/05/virtualbox-resize-hard-disk-error-vbox_e_not_supported.html
3. SDL1.2 installation support:
- sudo apt-get install libsdl1.2–dev
- https://linuxfromscratch.org/blfs/view/svn/multimedia/sdl.html
4. FFMPEG support:
- http://dranger.com/ffmpeg/end.html
- Downloaded ffmpeg-4.1.4-i686-static.tar.xz.md5 file from 
- https://www.johnvansickle.com/ffmpeg/old-releases/
- Extracted tar file to /home/litmus/FFMPEG
5. Period - Deadline tradeoff:
- https://www.rtx.ece.vt.edu/resources/Files/chantem08jul.pdf

## Execution Instructions
1. Compilation:
- make clean
- make all
- gcc -m64 video_proc.o -L/opt/liblitmus -llitmus -lSDL -lavformat -lavcodec -lswscale -lswresample -lavutil -lm -lpthread -o video_proc
2. Execution:
- ./video_proc
3. Set & Switch schedulers:
- showsched
- setsched <scheduler_name>
4. Tracing & Visualization
- mkdir <directory_name> cd <directory_name> st-trace-schedule & st-draw *.bin
- Hit enter to stop
- st-job-stats *.bin | head
- evince *.pdf or View on preview
- On another tab, run the program you want after starting the trace Multiple tests → Navigate to the respective folder
5. To find & kill trouble-causing tasks if any:
- ps -ef
- ps -ef | grep video_proc kill -9 <task_id>

## Evaluation 
In this section, color coding represents each set of tests conducted & their corresponding results.
<img width="802" alt="Screen Shot 2022-05-17 at 7 05 36 PM" src="https://user-images.githubusercontent.com/28973352/168931575-78d7e2d7-068d-468c-a23c-e63fe96ac5c5.png">
<img width="825" alt="Screen Shot 2022-05-17 at 7 07 05 PM" src="https://user-images.githubusercontent.com/28973352/168931687-828fe12a-4ee9-4332-bf38-d3baffef0461.png">

## Visualization Results (Sample)
<img width="506" alt="Screen Shot 2022-05-17 at 6 42 20 PM" src="https://user-images.githubusercontent.com/28973352/168930810-a6d60208-924f-457a-8c00-db48764f0afd.png">
<img width="504" alt="Screen Shot 2022-05-17 at 7 08 16 PM" src="https://user-images.githubusercontent.com/28973352/168931760-db24580b-2ddc-44ca-8823-a1d522230a1f.png">
