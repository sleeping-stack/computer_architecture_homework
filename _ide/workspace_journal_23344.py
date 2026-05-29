# 2026-05-29T08:17:40.943631400
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

comp = client.get_component(name="app_parallel_IO")
status = comp.clean()

platform = client.get_component(name="platform_new")
status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = comp.clean()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

vitis.dispose()

