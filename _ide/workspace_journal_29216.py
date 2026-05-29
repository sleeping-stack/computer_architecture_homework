# 2026-05-29T09:44:36.568026700
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

vitis.dispose()

