Major changes:
- expose SDL_DYNAMIC_API (SDL_DYNAPI) (ae1585de941eb1a50ffc0402b65f9e43ae07302a + 51c55497fe80c1a797e86334c56fdf378e6f8e02 + a541b7bb0141a8aa332b2174dd7d65bbda23a3b9 + ff4c8b98ce5ed65aa9ed6145a1fb826bde5bdb4a + 67e3582f3d7b4e85019278fa6abb4b509dce528b + 9f8f7648cee196f8f635a10464c1c60758f9d1bd)
- expose the options of SDL_interal.h (f3f67e9e612a0a21c3e401f9fe9ab5bf22bd3b4b + 798bde6358eb59586a4e0fb4c500f4d1c737849e + 7b8b9e82b2dfe16cafc6ab7317744a356e526366)
- add option to enable/disable SDL_BLIT_SLOW (61855fc35c11ed8575ffffbb648f95477d997bee)
- add option to enable/disable blit with modulo/alpha (SDL_BLIT_TRANSFORM) (e00e9a4f682bad39cce7c03d7f8786113257393b)
- add option to enable/disable verbose errors (c56c7206fadefe9df406181cfcc6b3fe5dab18f1 + 887a3392fb9f03c400762c2f626ab7eb27917e50)
- add option SDL_SANITIZE_ACCESS (3002789c7d9137f288be193cd003b76c76b98c01 + 90f2c195e6fe0206499dddaa463b87cd03a6c185)
  1. assume SDL_GetTicks64/SDL_GetPerformanceCounter/SDL_GetPerformanceFrequency and SDL_Delay are not called before SDL_TicksInit
- add option to enable/disable audio resampler (8b04d76bcbbae56be9432d645efae5f0b8e14e2f + e67cbee778d2142e8493f971a36d7af10bcb4cc1 + 0ab351d753504700ff331f8971f5885e7af5b8e8)
- add option to enable/disable the override of the memory-allocators (SDL_DYN_MEMFUNCS) (37cf21dcac3511df9379935f02843c7caaef4f74)

Minor changes:
- handle the joystick subsystem like the others (2adda5b42f4733470d0db47981b753a185c44031 + 6ddd5bdebe8dbec382795be01328a1ab2ba10469)
- sync configuration report (2664f550c7aa20aa28e627f945266b8969e9de69 + 9bda7d077acd3c0f4edc6a90fd911bb7afd0559b)
- cleanup test configuration (f8a63d295d1ed6c36d5a4180542dc4730ad3fd12 + 654eef8f7ba794360a9543ef6a3cc5d064d8522f + 89d6408f6aee0c2e40f80f296b1745f142ec96c8)
- add option to enable/disable logging (66fcc1a70cf7af2dc6554071e8993e168dd48d81 + 7213a1683838d8db100bddfd5fc5b1c30b9f3df9 + a451eb2827020ec2a6d95a87f7c722c3341fdb0c + 24637692c62fa902398ac903b46a68b9ac6e512e + 5860b778795c24aa128c231e96e52dce2df14541 + 6fb7859b77b5a4c78b5dba90d0aab602e6d430f3 + 41d05015d991cc3ad8e44ccdcb3a14d0ffaa002b + aa4aea46bf9c33b2581155b7652450794480b1e0 + 3b1aa5de168ec1b8cbd250a52c652e3784bd0bff)
- allow building without 'file' subsystem (976215319d8ed419f473475ba1525f062753eb72 + 6c3e1e8106bb048bfcc5e5d8d93d5bdb53e5e1a4 + 20259e4c9d1c21d7f3765c0e4f48e395d2c13092)
- do not report dummy haptic subsystem as ON (dec3267bb8f863edc39b49499be38f8cdb672f9b)
- make dummy/diskaudio and dummy video OFF by default (f2c73a7fb0c445074562a504b3badeea22a38568 + 415d87935c60fe98b86b8990d89d4f4277ea6177)
- make virtual joystick OFF by default (2821272508d9fdf65db274ec314bf4323e03ed4f)
- make offscreen video OFF by default (41ac06835dfc24ddc4a8d6e1d37739253d22279e)
- report if subsystem is live or just a dummy (b0e6a3e5d8f0cd734952364e197dd3a739314cb3 + 3418aab14301cfa3c85163b529ed8e3aa433fe4d)
- upload created artifacts on push and pull_request (bf08b8f89ea2e37dee0d7e63386116e763f5eff6 + 69091cc9ff23801b731e7d664266154b8993cf4f)
- get rid of SDL_AudioDriver.desc (2b9a7d7601f756c3fa4621704ad7388f0ccc2417 + b4e62c06a29ec4ee9b0522e545c7f5fd5e871bd3 + 4dd9c3a4bebd96f411b71ca518138a3ac7d137e0)
- get rid of SDL_VideoDriver.desc (990453165480988f66ae47b13dad2835480d7dee + 57e79ab7c20977b386d6bf5e56dd2f792f2d579d + )
- replace OnlyHasDefault*Device flags with PreventSimultaneousOpens (e9d327dcac6ffb7e5033539933e029b8fc99e9ba + 56e6db8d93fe1b63a72906ecb72c987727d33936 + a922acdd7cead695061786db984a33f2e76c3ccd)
- update SDL_CreateRGBSurface* interface (2b389c5133a0151a73ca6a4af6f4e26722a2abe6)
- do not use SDL_GetError internally (4da1368a2bbd99e92e61455c037bed17952fea63 + f2bb79190f57d19ee6893aed1ca89a04bbb5b1ec)
- do not use SDL_asprintf internally (4f9378bab2005448ac245016006d5ac9b40f9d2a)
- Turn off debug code in normal release (9f81725966733df9bee2cc12ad6d2ac9862d04ce)
- hide SDL_GenerateAssertionReport in case SDL_ASSERT_LEVEL is 0 (9d4f4056fa836e0f5cd264da27f53c2dc8deedd8)
- sync handling of subsystems III. (threads) (4bf613e96b15d3087b43f10997b78d0932331e39 + ff609444f1c6d91186999d7459b42d1fe1aafdf3 + eb539788a342ba14b07c65568211bf5f3e75ad40)
- simplify simulated vsync (d744aaf) (8c670433a51c111aa4c75d74e135f567c51a1267)
- improve DUFFS_LOOP_124 (fddbd55c3d992ac361362e6ecbf3a7ffb4e07659)
- prevent redefinition of _USE_MATH_DEFINES (daece4f51048120779df1bed50dc3b4786a630d7)
- revert part of Clang-Tidy fixes (SDL_audio.c) (f3cdb1db49c1e75252e02becf27f8b90a37fec51)
- simplify memory handling (c51c2e6e0be8138bb91c1836656d5677a13d05e4 + ae8ba6fc19b4f94d3d6cf06b28884fd95e3d8f1b)
- minor cleanup SDL_GetPrefPath of unix/SDL_sysfilesystem.c (202cd041545e8f239a6a05214596afff689d0c24 + f47b03501cfac63e04ef9a167682a7a2b2bb1f13)
- sort the library function-names to check (e361bda5a19230182ad74c07159c9e02ec5e52d0)
- cleanup SDL_GetBasePath of windows/SDL_sysfilesystem.c (97004742179c468411a12fa63e3787105412f80f)
- cleanup SDL_GetPrefPath of windows/SDL_sysfilesystem.c (f0c63f403f3bb787beced2588de9d01e2742e872 + 8312e25445729f4fceb6aef0c6e21cce8d2a7e49)
- revert 'Retry to open the clipboard in case another application has it open' (2515e71a17717f7e329977653f20db1a9fe45f19)
- revert 'Don't crash if SDL_MapRGB() and SDL_MapRGBA() are passed a NULL format (bbb0affb35fc535813220dbb874f23c1c176f733)
- reduce footprint of 'Fix automated tests using the dummy video driver'/f9dc49c21c99cd6eb63d80d60b6e706a51234b46 (46904534cfc2ba0f4c5202b2b4e01245f8473b26)

Bugfixes:
- really disable assertions if it is set to 'disabled' (7aa402616e2f072a96497f5a5648b739e05849e6 + 632f1969ab2085f7f9a5632d6a715f6cb149a193 + bda335ce5920d850dfaa266f499d846d1d4f1f1e)
- fix DUFFS_LOOP4/8 to handle width of zero (or less) (8d18c5e59856bd5e46b93d9de4550d1d0b89dc26)
- fix vita error message when the port can not be opened (5c75e0c8577a4854d6c77e574bf56074d5bb542f)
- fix memory handling of vita-audio (7f4f1b174c5aeddaaf99d14739330efa8e6da20f)
- fix memory leak in windows/SDL_windowshaptic.c (da0323960b23913f6aec422f385b8ded56867510)
- fix possible NULL-pointer errors due to allocation error (7af2b40ee1d40435c77b469dc5118c08575c20ad)
- fix memory leaks in psp/ps2audio (01a453c59ac13281fa2416e48517886c13fbade3)
- fix error handling in get_usb_string (21a61a0b7eb70cb27e72afc6b1081fd4e441394c)
- add missing sscanf check to configure (3effa86076e5609ea8c7786c6fc05d5a65b7144c)