Major changes:
- expose SDL_DYNAMIC_API (SDL_DYNAPI) (ae1585de941eb1a50ffc0402b65f9e43ae07302a + 51c55497fe80c1a797e86334c56fdf378e6f8e02 + a541b7bb0141a8aa332b2174dd7d65bbda23a3b9 + ff4c8b98ce5ed65aa9ed6145a1fb826bde5bdb4a + 67e3582f3d7b4e85019278fa6abb4b509dce528b)
- expose the options of SDL_interal.h (f3f67e9e612a0a21c3e401f9fe9ab5bf22bd3b4b)
- add option to enable/disable verbose errors (c56c7206fadefe9df406181cfcc6b3fe5dab18f1 + 887a3392fb9f03c400762c2f626ab7eb27917e50)

Minor changes:
- handle the joystick subsystem like the others (2adda5b42f4733470d0db47981b753a185c44031)
- sync configuration report (2664f550c7aa20aa28e627f945266b8969e9de69)
- cleanup test configuration (f8a63d295d1ed6c36d5a4180542dc4730ad3fd12 + 654eef8f7ba794360a9543ef6a3cc5d064d8522f)
- add option to enable/disable logging (66fcc1a70cf7af2dc6554071e8993e168dd48d81 + 7213a1683838d8db100bddfd5fc5b1c30b9f3df9 + a451eb2827020ec2a6d95a87f7c722c3341fdb0c + 24637692c62fa902398ac903b46a68b9ac6e512e + 5860b778795c24aa128c231e96e52dce2df14541 + 6fb7859b77b5a4c78b5dba90d0aab602e6d430f3)
- allow building without 'file' subsystem (976215319d8ed419f473475ba1525f062753eb72 + 6c3e1e8106bb048bfcc5e5d8d93d5bdb53e5e1a4 + 20259e4c9d1c21d7f3765c0e4f48e395d2c13092)
- do not report dummy haptic subsystem as ON (dec3267bb8f863edc39b49499be38f8cdb672f9b)
- make dummy/diskaudio and dummy video OFF by default (f2c73a7fb0c445074562a504b3badeea22a38568)
- make virtual joystick OFF by default (2821272508d9fdf65db274ec314bf4323e03ed4f)
- report if subsystem is live or just a dummy (b0e6a3e5d8f0cd734952364e197dd3a739314cb3 + 3418aab14301cfa3c85163b529ed8e3aa433fe4d)
- upload created artifacts on push and pull_request (bf08b8f89ea2e37dee0d7e63386116e763f5eff6)
- get rid of SDL_AudioDriver.desc (2b9a7d7601f756c3fa4621704ad7388f0ccc2417)
- get rid of SDL_VideoDriver.desc (990453165480988f66ae47b13dad2835480d7dee)
- replace OnlyHasDefault*Device flags with PreventSimultaneousOpens (e9d327dcac6ffb7e5033539933e029b8fc99e9ba)
- update SDL_CreateRGBSurface* interface (2b389c5133a0151a73ca6a4af6f4e26722a2abe6)
- do not use SDL_GetError internally (4da1368a2bbd99e92e61455c037bed17952fea63)
- Turn off debug code in normal release (9f81725966733df9bee2cc12ad6d2ac9862d04ce)

Bugfixes:
- really disable assertions if it is set to 'disabled' (7aa402616e2f072a96497f5a5648b739e05849e6 + 632f1969ab2085f7f9a5632d6a715f6cb149a193)
