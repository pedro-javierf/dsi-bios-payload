# DSi ARM7 Boot ROM dumper payload

This repo contains three directories:

* `payload`: the actual payload code. `arm7` is the interesting stuff, `arm9`
  is unused placeholder code.
* `dumper`: Installs the payload and then causes a reset. (Also has a version
  to just hang forever, useful for testing and tuning an EMFI glitching setup.
* `undtest`: Random test to see if hooking the und. insn. handler works
  correctly. Not really useful anymore, except it has all the build script
  files, all the other directories have symlinks to these. Yes I was lazy.
