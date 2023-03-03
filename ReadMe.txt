SimpleMiner:

The goal of this project was to make a clone of Minecraft and through that learn how to manager large worlds and generate them through procedural generation. It was also a good exercise in implementing various optimizations in order to reduce the render time of large worlds.

Key Features:
1. Load 900 chunks at once with a total of 1 million+ verts with a stable 60 fps.
2. Terrain generation using Perlin noise.
3. Surface lighting using block propagation technique.
4. Translucent water and vertex animation to simulate waves.
5. Corrective physics for player vs the world.
6. Different camera modes such as first person, over the shoulder, fixed angle.
7. World generation is multithreaded using an engine side Job system.
