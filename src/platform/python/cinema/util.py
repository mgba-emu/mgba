def dict_merge(a, b):
    for key, value in b.items():
        if isinstance(value, dict):
            if key in a:
                dict_merge(a[key], value)
            else:
                a[key] = dict(value)
        else:
            a[key] = value
