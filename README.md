# Silicon Labs firmware builder repository

This repository contains Dockerfiles and GitHub actions that build Silicon Labs firmware. It also hosts unofficial Zigbee Coordinator and Thread (OpenThread) firmware builds that community members can experiment with at your own risk.

The firmware builder uses the Silicon Labs [Gecko SDK (GSDK)](https://github.com/SiliconLabs/gecko_sdk) and proprietary Silicon Labs tools such as the Silicon Labs Configurator (slc) and the Simplicity Commander standalone utility. This is a fork of the [Silabs firmware builder by Nabu Casa](https://github.com/NabuCasa/silabs-firmware-builder), adding support for additional radio adapter hardware models.

Again, please note that the pre-compiled firmware builds hosted in this repository are both unofficial and experimental or cutting-edge releases with  minimal testing which may brick your radio adapter so that it requires manual recovery via a compatible debug probe adapter, however, the builds offered via the Web Flasher are the latest versions recommended by the community.


## Legacy firmware builds

If you are looking for Gecko v7 builds, they can be found here
[Legacy Gecko SDK builds](https://github.com/darkxst/silabs-firmware-builder/tree/main/firmware_builds)

## Supported hardware:
* Sonoff ZBDongle-E by ITead (based on EFR32MG21)
* Sonoff Dongle Plus Mg24 (based on EFR32MG24)
* Sonoff iHost by ITead (also based on EFR32MG21 and uses the same firmware as Sonoff ZBDongle-E)
* EasyIot ZB-GW04 v1.1 and v1.2 (based on EFR32MG21)
* SMLIGHT SLZB-07 (which may require unlocked [bootloader](https://github.com/darkxst/silabs-firmware-builder/raw/main/firmware_builds/slzb-07/BTL_SLZB07.gbl) first) (based on EFR32MG21)
* SMLIGHT SLZB-07Mg24 (based on EFR32MG24)
* SMLIGHT SLZB-06M (based on EFR32MG21)
* SMLIGHT SLZB-06Mg24 (based on EFR32MG24)
* SMLIGHT SLZB-06Mg26 (based on EFR32MG26)
* Aeotec Zi-Stick (model “ZGA008” based on EFR32MG21)
* Sparkfun Things Matter MGM240P (requires [bootloader](https://github.com/darkxst/silabs-firmware-builder/blob/main/firmware_builds/mgm240p/bootloader-uart-xmodem_NCP.hex) to be flashed first using Silabs [Simplicity Commander](https://community.silabs.com/s/article/simplicity-commander?language=en_US)) (based on EFR32MG24)
* SeeedStudio Xiao MG24 (based on EFR32MG24), requires bootloader to be [flashed with OpenOCD](https://github.com/darkxst/silabs-firmware-builder/issues/188#issuecomment-2670476887)
* Xiaomi Multimode Gateway 2/Mi Smart Hub Pro 2 (based on EFR32MG21)
* Elelabs Zigbee USB Adapter ELU013 (based on EFR32MG13P) [legacy only, 7.4.x builds]
* Elelabs Zigbee Raspberry Pi Shield ELR023 (based on EFR32MG13P) [legacy only, 7.4.x builds]

## Different firmware variants

Three network protocol application firmware variants are available:

* **EmberZNet NCP** = Zigbee NCP (Network Co-Processor) is used as a dedicated Zigbee Coordinator for Zigbee-only environments, for direct use with Zigbee2MQTT, Home Assistant's ZHA integration, other Zigpy based Zigbee Gateway implementations, or other Zigbee gateways/frameworks that support the EZSP (EmberZNet Serial Protocol) interface.
* **OpenThread RCP** firmware (experimental) = This [Thread](https://en.wikipedia.org/wiki/Thread_(network_protocol)) RCP (Radio Co-Processor) is used directly as a dedicated Thread Border Router in Thread-only environments, used for [OpenThread Border Router add-on](https://github.com/home-assistant/addons/blob/master/openthread_border_router/DOCS.md) or wpantund.
* **RCP Multi-PAN** (no longer recommended) = Multiprotocol firmware for concurrent communication over Zigbee and Thread via Home Assistant [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

**Note!** RCP MultiPAN in multiprotocol mode is no longer available. Running multi-protocol with multiple active networks on a single radio adapter has proven unstable when using Zigbee and Thread simultaneously. It also increases software component complexity. If you're currently using RCP Multi-PAN, it's highly recommended to migrate to separate dedicated radio adapters (using Zigbee NCP and Thread RCP firmware respectively), even if your current setup appears to be working fine. Note that we do not support Sonoff's re-branded multi-PAN attempts either.

External reference explaining these different co-processor designs at a high level:
  * https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md
  * https://openthread.io/platforms/co-processor

## Web Flasher
Flash directly from your browser (only Chrome and Edge supported) [SL Web Flasher](https://darkxst.github.io/silabs-firmware-builder/)

Read this [blog post](https://dialedin.com.au/blog/sonoff-zbdongle-e-rcp-firmware) for more details and instructions for using RCP Multi-pan firmware.

## Home Assistant Add-on
**RCP MultiPan firmware ONLY**  
You can install this HA add-on to keep your dongle up to date with latest the RCP  Multi-Pan firmware. This is only for use if you are using the [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

[Multipan Flasher Addon](https://github.com/darkxst/multipan_flasher/tree/main)

It can also be used for the initial conversion to MultiPan firmware, however you will need to disable ZHA or Zigbee2MQTT while you do this initial flash. You can then install and configure the Silabs Multiprotocol Add-on.

## Pre-Compiled Firmware
Firmware builds can be found in the [firmware_builds](https://github.com/darkxst/silabs-firmware-builder/tree/main/firmware_builds) folder.

`ncp-uart-hw-` EmberZnet pure Zigbee  
`rcp-uart-802154-` RCP MultiPan  
`ot-rcp-` OpenThread Only  

ZBDongle-E and ZB-GW04 v1.1 do not support hardware flow control. Yellow, SkyConnect and ZB-GW04 v1.2 are built with hardware flow control. Various baudrates are available as listed at the end of the filename.

Use NabuCasa's [Universal-Silabs-Flasher](https://github.com/NabuCasa/universal-silabs-flasher) to flash the `.gbl` files.


## Building locally with VSCode

To build firmware locally, simply open this repository in the included devcontainer.

You can then use the `Run Task` feature and select "Select Manifest" to choose the manifest you want to build.

To build manually and keep build artifacts, run:
```sh
./tools/buildit.sh <manifest.yaml> --no-clean-build-dir
```

## Support

If you would like to help support further development of my `Silabs firmware` projects consider buying me a coffee!

<a href="https://www.buymeacoffee.com/darkxst" target="_blank"><img src="img/blue-button.png" alt="Buy Me A Coffee" height="41" width="174"></a>
