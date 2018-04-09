provider "aws" {
    access_key = "${var.access_key}"
    secret_key = "${var.secret_key}"
    region = "eu-central-1"
}

provider "aws" {
    alias = "us"
    access_key = "${var.access_key}"
    secret_key = "${var.secret_key}"
    region = "us-west-2"
}

resource "aws_instance" "eu_c48xlarge" {
    count = 2
    ami = "ami-0dbc3262"
    instance_type = "c4.8xlarge"
    key_name = "${var.key_name}"
}
