# Cloth (Forked)

This repository is forked from [marcono/cloth](https://github.com/marcono/cloth).

## Modifications
- Made executable within a Docker container.
- Added lease functionality.
- Implemented features inspired by [ElementsProject/lightning](https://github.com/ElementsProject/lightning) (MIT License).
- Utilized data sourced from [1ml.com](https://1ml.com/).

## 🚀 How to Run the Simulator

This simulator runs inside a Docker container. Follow the steps below to execute it.

### 1. Start the container

```bash
docker compose up -d
```

### 2. Enter the container

Check the container name with:

```bash
docker ps
```

Then connect to the container (replace `cloth-app-1` with your actual container name if different):

```bash
docker exec -it cloth-app-1 /bin/bash
```

### 3. Run the simulator

Make sure the `output/` directory exists:

```bash
ls output/
```

Then execute the simulator:

```bash
./run-simulation.sh 1 output/
```

- The first argument (`1`) specifies the simulation ID or configuration number.
- The second argument (`output/`) is the directory where results will be saved.


## Publications

If you use or reference this work, please cite the following publication:

- 金丸剛・小島英春, "ライトニングネットワークシミュレータの流動性に関する改良", 信学技報, vol. 124, no. 310, NS2024-163, pp. 116-121, 2024年12月.

## License
Licensed under the GNU General Public License v3.0 ([LICENSE](LICENSE)).