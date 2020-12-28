# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [v3.0.0-alpha.0.2] - 2020-12-27
### Added

- Ball particles for some of the Apricorn Balls by [@huderlem](https://github.com/huderlem).
- Flower animations for flowers in Goldenrod City gym and Pretty Petal Flower Shop.

### Changed
- Headbutt tree tile, so it doesn't look weird when layered over a wooden fence.

### Fixed
- Dept. Store rooftop sale NPCs no longer appear when there isn't a sale (and right now, there is never a sale).
- Non-modern build no longer complains about unused song files.
- Fixed a bunch of undefined behaviors, which were causing crashes in emulators that don't know how to handle them. This fixes the game crash on staircases in My Boy!.
- Sanitized Radio Tower tileset for incorrect metatile behaviors. This stops the warp to a black map when interacting with machines and phones.
- Certain headbutt trees (specifically those under the tops of big trees) could not be headbutted; this has been fixed.
- Floria (the younger flower shop sister) no longer teleports around.
- Fixed minor text issues.

## [v3.0.0-alpha.0.1] - 2020-12-25
### Added

- Initial release, up through Sudowoodo event on Route 36

[unreleased]: https://github.com/Sierraffinity/CrystalDust/compare/v3.0.0-alpha.0.2...HEAD
[v3.0.0-alpha.0.2]: https://github.com/Sierraffinity/CrystalDust/releases/tag/v3.0.0-alpha.0.2
[v3.0.0-alpha.0.1]: https://github.com/Sierraffinity/CrystalDust/releases/tag/v3.0.0-alpha.0.1