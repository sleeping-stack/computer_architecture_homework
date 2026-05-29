# 2026-05-29T09:34:36.884311400
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

platform = client.get_component(name="platform_new")
status = platform.build()

comp = client.get_component(name="app_parallel_IO")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

vitis.dispose()

