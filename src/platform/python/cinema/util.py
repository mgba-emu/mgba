def dictMerge(a, b):
    for key, value in b.items():
        if isinstance(value, dict):
            if key in a:
                dictMerge(a[key], value)
            else:
                a[key] = dict(value)
        else:
            a[key] = value
