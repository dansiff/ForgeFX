Create a Post Process Material (M_PPOutline):
1. Material Domain: Post Process.
2. Blendable Location: Before Tonemapping.
3. Add SceneTexture:PostProcessInput0 (Base).
4. Add Custom Depth pass: Enable CustomDepth-Stencil in Project Settings.
5. Read SceneTexture:CustomStencil (node) or CustomDepth and compare to value (e.g.252).
6. If match: output emissive color * intensity or outline using Sobel on CustomDepth.
7. Assign material to a global PostProcessVolume (Infinite Extent) in the level.
