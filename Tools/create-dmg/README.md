## create-dmg
A small fork of create-dmg, which has an edited `hidutil detach` call. Why the fork?

- GitHub Actions will sometimes just die on trying to detach properly.
- We don't want to have to retry the entire create-dmg flow just to get it right.
- Forking is just easier to get the inner loop for what we want.

See the project here: https://github.com/create-dmg/create-dmg
