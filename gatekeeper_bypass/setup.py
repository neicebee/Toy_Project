from setuptools import setup

setup(
    name="my_boop",
    version="1.4.0",
    description="test my boop",
    author="soso123",
    author_email="f1r3_r41n@hansung.ac.kr",
    packages=["my_boop"],
    package_data={"my_boop": ["Boop.zip"]},
    include_package_data=True,
    zip_safe=False,
)