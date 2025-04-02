<h1 align="center">
  <a href="https://github.com/aurixos/os">
    <img src="docs/images/logo.png" alt="Logo" width="100" height="100">
  </a>
</h1>

<div align="center">
  AurixOS
  <br />
  <br />
  <a href="https://github.com/aurixos/os/issues/new?assignees=&labels=bug&template=01_BUG_REPORT.md&title=bug%3A+">Report a Bug</a>
  ·
  <a href="https://github.com/aurixos/os/issues/new?assignees=&labels=enhancement&template=02_FEATURE_REQUEST.md&title=feat%3A+">Request a Feature</a>
  .
  <a href="https://github.com/aurixos/os/issues/new?assignees=&labels=question&template=04_SUPPORT_QUESTION.md&title=support%3A+">Ask a Question</a>
</div>
<br />
<div align="center">

[![Project license](https://img.shields.io/github/license/aurixos/os.svg?style=flat-square)](LICENSE)
[![Discord Chat](https://img.shields.io/discord/1234051470182055988)](https://discord.com/invite/hgbZ6wFP4n)
</div>

<details open="open">
<summary>Table of Contents</summary>

- [About](#about)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building](#building)
- [Running](#running)
- [Support](#support)
- [Contributing](#contributing)
- [Authors & contributors](#authors--contributors)
- [Security](#security)
- [License](#license)
- [Acknowledgements](#acknowledgements)

</details>

---

## About

AurixOS is a general-purpose operating system, meant to take security, performance and usability to the next level.

<!-- 
<details>
<summary>Screenshots</summary>
<br>

|                               Title                               |
| :---------------------------------------------------------------: |
| <img src="docs/images/screenshot.png" title="Title" width="100%"> |
</details>
-->

## Getting Started

Latest official releases of AurixOS are available for download [here](https://github.com/aurixos/os/releases).

### Prerequisites

If you have [Homebrew](https://brew.sh) installed, simply run `brew bundle` to install **all** required build dependencies (incl. packages to build AurixOS for every supported architecture).
Alternatively, read the [Brewfile](Brewfile) in the root of this repository and install the required (and optional) packages manually.

### Building

To build AurixOS, simply run `make` in the root directory of this repository.
To change the build target, run `make ARCH=arch PLATFORM=platform`, where `arch` is the target architecture, and `platform` is the target platform (Default is set to `x86_64`/`generic-pc`).

#### Possible/planned targets:
##### i686 / x86_64
- `generic-pc`

##### arm32 (not available yet)
- `raspi2`

##### aarch64
- `raspi3` (not available yet)
- `raspi4` (limited support)
- `raspi5` (not available yet)

> [!NOTE]  
> On x86(_64) architectures, a UEFI bootloader will be automatically built and packaged together together with a legacy stub. On arm32 and aarch64, invoke Make with `NOUEFI=n` modifier to create a UEFI-capable image.

Inputting an invalid architecture/platform combination will result in an error.

Building AurixOS images is also supported with Make recipes:
- `livecd` builds a CD-ROM image (.iso)
- `livehdd` builds a HDD image (.img)
- `livesd` builds an SD Card image (.img)

Please note that not all bootable images can be created for all platforms (eg. `rpi4` only supports creating an SD Card image). Any attempt at creating an unsupported image will result in an error.

All generated images reside in the `release/` directory and follow this naming scheme:
`aurixos-live[MEDIA]-[GIT_REV]_[ARCH]-[PLATFORM].iso`

`make help` shows you all available build options

## Running

To run AurixOS on QEMU, run `make run` (optionally, append `ARCH=arch PLATFORM=platform` if you're building/running AurixOS on an architecture other than x86_64).

Alternatively, you can boot AurixOS on a physical machine by running `dd if=release/<usb-image>.img of=/dev/<target USB drive> bs=1M` and booting off of this USB drive.

## Support

If you have any questions, feel free to open a [GitHub issue](https://github.com/aurixos/os/issues/new?assignees=&labels=question&template=04_SUPPORT_QUESTION.md&title=support%3A+).
Alternatively, you can visit us:
- on our [Discord server](https://discord.com/invite/hgbZ6wFP4n)
- on #aurixos at [irc.libera.chat](https://libera.chat/) ([Kiwi IRC](https://kiwiirc.com/client/irc.libera.chat/?&theme=cli#aurixos))

## Contributing

First off, thanks for taking the time to contribute! Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make will benefit everybody else and are **greatly appreciated**.

Please read [our contribution guidelines](docs/CONTRIBUTING.md), and thank you for being involved!

## Authors & contributors

For a full list of all authors and contributors, see [the contributors page](https://github.com/aurixos/os/contributors).

## Security

AurixOS follows good practices of security, but 100% security cannot be assured.
AurixOS is provided **"as is"** without any **warranty**. Use at your own risk.

_For more information and to report security issues, please refer to our [security documentation](docs/SECURITY.md)._

## License

This project is licensed under the **MIT license**.

See [LICENSE](LICENSE) for more information.
