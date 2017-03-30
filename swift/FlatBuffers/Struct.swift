//
//  Struct.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//

/// <summary>
/// All structs in the generated code derive from this class, and add their own accessors.
/// </summary>
public struct Struct {
    public let bb_pos : Int
    public let bb : ByteBuffer
}
