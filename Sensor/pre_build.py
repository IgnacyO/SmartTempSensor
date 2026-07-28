from os.path import exists

Import("env")

if exists(".env"):
    with open(".env") as f:
        for line in f:
            line = line.strip()

            if not line or line.startswith("#"):
                continue

            key, value = line.split("=", 1)

            env.Append(CPPDEFINES=[(key, '\\"{}\\"'.format(value))])
