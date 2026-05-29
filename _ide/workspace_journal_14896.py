# 2026-05-29T09:09:13.848477200
import vitis

client = vitis.create_client()
client.set_workspace(path="MicroBlaze_AC850.sw")

platform = client.get_component(name="platform_new")
status = platform.build()

comp = client.get_component(name="app_hello_world")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp = client.get_component(name="app_parallel_IO")
comp.build()

client.delete_component(name="app_hello_world")

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

status = platform.build()

comp.build()

vitis.dispose()

