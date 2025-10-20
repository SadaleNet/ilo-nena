# ilo "Bootloader" li seme?

* kepeken ilo ni la jan li ken pilin e nena "pana" li pana e sona sin tawa ilo nena kepeken nasin "USB".
* lipu `bootloader.patch` li ante e lipu ni kepeken ilo `git apply`: https://github.com/cnlohr/rv003usb/tree/35e0f688a9a041ed8cec37fcbb458df095dc0d79 ni la sina ken pali e lipu `bootloader.bin`
* tenpo pini la mi kin li pali e lipu `bootloader.bin`. sina ken kepeken e ona e lipu `bootloader.patch` ala.
* sina ken pana e ilo ni tawa ilo nena kepeken ilo tu ni lon tenpo sama: ilo "WCH-LinkE" en ilo "minichlink". o kepeken nimi ni: `./minichlink -a -w bootloader.bin bootloader -B`
	* (PALI PI TENPO KAMA: jan Sate o pana e sitelen lon ma ni! ma ni o jo e sitelen pi ilo WCH-LinkE en ilo nena mi)

## ken

jan Sate li pali ala e ilo ni. jan "CNLohr" li pali e ona. ken ona li ken "MIT License".

# Bootloader of ilo nena

* Provides USB flashing functionality by holding the "pana" key upon boot
* Requires WCH-LinkE to flash the bootloader itself
* `bootloader.patch` is to be applied with `git apply` against the following repo's commit: https://github.com/cnlohr/rv003usb/tree/35e0f688a9a041ed8cec37fcbb458df095dc0d79 . You can build `bootloader.bin` with it on your own. The patch enables the hold-button-to-enter-bootloader functionality.
* You can also use the `bootloader.bin` pre-built binary in this folder instead of building the bootloader on your own
* To flash, WCH-LinkE and minichlink are required. Run the following command after connecting ilo nena with WCH-LinkE: `./minichlink -a -w bootloader.bin bootloader -B`
	* (TODO: show photo of hardware connection for flashing here)

## License

I'm not the author of the bootloader. It's created by CNLohr released under MIT License.
