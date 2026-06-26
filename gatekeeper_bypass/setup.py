from setuptools import setup

setup(
    name="my_soso_boop",
    version="1.4.3",
    description="test boop",
    author="soso123",
    author_email="f1r3_r41n@hansung.ac.kr",
    packages=["my_soso_boop"],
    package_data={"my_soso_boop": ["Boop.zip"]},
    include_package_data=True,
    zip_safe=False,
)