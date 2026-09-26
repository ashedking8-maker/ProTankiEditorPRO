# 0.5.18 source candidate

Patch against original 0.5.17, including the earlier Release CTest fix. No editor map writes introduced by UI/rasterizer changes. Run `python tools/check_source_consistency.py` and `python -m unittest discover -s tests -p "test_*audit.py" -v`; then GitHub Actions Windows x64 Release CTest and package. Inspect Bridge 1 in normal and Unlit mode from ABOVE and BELOW. Confirm Sprite / solid 3DS and unknown XML remain separately labeled and original map data is unchanged. Windows runtime rendering not verified in this container.
