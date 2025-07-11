You need to install the neccesary packages to make this work

```bash
sudo apt install libpython3-dev pybind11-dev nanobind-dev python3
```

To test all this I use uv, which can be installed with
```bash
$ curl -LsSf https://astral.sh/uv/install.sh | sh
```

and to test the pybind bindings go into the folder with the module the following command can be executed with bash
```bash
uv venv && uv pip install . && source .venv/bin/activate && python -c "import matrix_pybind; x = matrix_pybind.add(2,3); print(f'{x=}')" && deactivate && rm -rf .venv/
```
or this one one with fish
```bash
uv venv && uv pip install . && source .venv/bin/activate.fish && python -c "import matrix_pybind; x = matrix_pybind.add(2,3); print(f'{x=}')" && deactivate && rm -rf .venv/
```

To test the pybind bindings use 
```bash
uv venv && uv pip install . && source .venv/bin/activate && python -c "import matrix_nanobind; x = matrix_nanobind.add(2,3); print(f'{x=}')" && deactivate &&  rm -rf .venv/
```
or with fish
```bash
uv venv && uv pip install . && source .venv/bin/activate.fish && python -c "import matrix_nanobind; x = matrix_nanobind.add(2,3); print(f'{x=}')" && deactivate &&  rm -rf .venv/
```