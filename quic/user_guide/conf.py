import os
import subprocess
# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Qualcomm Hexagon QEMU-VP User Guide'
copyright = 'Qualcomm Technologies, Inc.'
author = 'Qualcomm Technologies, Inc.'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'myst_parser',
    'sphinx_rtd_theme',
    'sphinx.ext.autosectionlabel',
    'linuxdoc.rstFlatTable'
]

autosectionlabel_prefix_document = True

templates_path = ['_templates']

exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', '_ignore']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

html_copy_source = False
html_show_sourcelink = False
html_show_sphinx = False

html_css_files = ['custom_css.css']

#myst extensions
myst_enable_extensions = [
    'amsmath',
    'attrs_inline',
    'attrs_block',
    'colon_fence',
    'deflist',
    'dollarmath',
    'fieldlist',
    'html_admonition',
    'html_image',
    #'linkify',
    'replacements',
    'smartquotes',
    'strikethrough',
    'substitution',
    'tasklist',
]

myst_heading_anchors = 6
