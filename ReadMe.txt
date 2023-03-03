Just regularly build and run. Nothing else required.

Time execution for tasks:
1) Time to generate a chunk using Perlin noise, etc. - Worst = 4.54ms, Average = 1.685ms
2) Time to load a chunk from disk, including RLE decompression -  Worst = 0.52ms, Average = 0.21ms
3) Time to save a chunk to disk, including RLE compression - Worst = 1.26ms, Average = 0.786ms
4) Time to rebuild a chunk’s CPU-side vertex array - Worst = 3.71ms, Average = 1.60ms
5) Time to resolve all dirty lighting each frame (first 20 frames) - Worst = 0.93ms, Average = 0.36ms`

Frame times:
				Debug	|	DebugInline	|	FastBreak	|	Release
1) While activating chunks	~47ms		~46ms			~15ms			~2.3ms
2) After world has stabilized	~2.3ms		~2.3ms			~0.7ms			~0.6ms


Note: For A04, the camera distance adjusting for over the shoulder and fixed angle tracking was not documented in the 
video recording for that assignment as it was implemented after recording the video.